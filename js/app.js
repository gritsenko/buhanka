/*
 * Дорога на восток — игровая логика прототипа.
 *
 * Прототип специально написан на чистом Vanilla JS — позднее логика будет
 * портирована на C++ для микроконтроллера ESP32 (LilyGO T-Display).
 *
 * Архитектура повторяет ограничения целевой платформы:
 *   - события игры читаются из data/events.json (на ESP32 файл будет
 *     скачиваться по Wi-Fi и парситься через ArduinoJson, поэтому поля
 *     зафиксированы: id, text, hint, sprite, choices[], hp_mod, food_mod,
 *     mood_mod, transport_mod, fuel_mod, next_id, unlock_friend[_2]);
 *   - кадры пиксель-арт анимаций — PNG-спрайт-шиты в assets/sprites/,
 *     описанные в data/sprites.json. Перед прошивкой ESP32 отдельный
 *     скрипт-конвертер переводит PNG в массивы RGB565 (или indexed+RLE)
 *     и линкует их как PROGMEM-данные;
 *   - две физические кнопки эмулируются через #btn-top и #btn-bottom
 *     (короткое нажатие — листать/подтвердить, длинное — статус/карта);
 *   - тач-интерфейс дублирует управление для запуска как PWA на телефоне.
 */

// ----- ДАННЫЕ ИГРЫ -----
// Заполняется после fetch('./data/events.json'). На ESP32 эквивалент —
// JsonDocument в RAM/PSRAM после парсинга ответа HTTP-клиента.
let eventsData = [];

// Манифест пиксель-арт спрайтов из data/sprites.json. Структура:
//   spritesData.sprites[name] = { src, frameWidth, frameHeight, frames, fps }
//   spritesData.transport_default = ['walk', 'bike', 'uaz']  // индекс = state.transport
// На ESP32 этот же манифест читает конвертер ассетов: каждый PNG переводится
// в массив RGB565 (или indexed+RLE) и линкуется в прошивку как PROGMEM-данные.
let spritesData = { sprites: {}, transport_default: [] };

// Масштаб спрайта в браузере: 1 пиксель спрайта = SPRITE_SCALE экранных пикселей.
// На ESP32 спрайт рендерится 1:1 — этот множитель только для PWA-отладки.
const SPRITE_SCALE = 5;

// ----- СОСТОЯНИЕ ИГРЫ -----
const state = {
    food: 100,
    fuel: 0,
    transport: 0, // 0 — пешком, 1 — велосипед, 2 — УАЗ
    currentEventId: 'start',
    party: [
        { id: 'hero',   name: 'Я',     icon: '🧑',     hp: 100, mood: 100, active: true  },
        { id: 'sashka', name: 'Сашка', icon: '👱‍♂️',   hp: 100, mood: 100, active: false },
        { id: 'ilya',   name: 'Илья',  icon: '🧔',     hp: 100, mood: 100, active: false },
        { id: 'maks',   name: 'Макс',  icon: '🧑‍🦲',   hp: 100, mood: 100, active: false }
    ]
};

let currentChoices = [];
let currentHint = '';
let selectedChoiceIndex = 0;
let isAnimating = false;

// ----- DOM -----
const foodBar        = document.getElementById('food-bar');
const fuelBar        = document.getElementById('fuel-bar');
const fuelContainer  = document.getElementById('fuel-container');
const partyPanel     = document.getElementById('party-panel');
const textArea       = document.getElementById('text-area');
const choicesArea    = document.getElementById('choices-area');

const statusOverlay  = document.getElementById('status-overlay');
const mapOverlay     = document.getElementById('map-overlay');
const animScreen     = document.getElementById('animation-screen');
const spriteEl       = document.getElementById('sprite');
const statusText     = document.getElementById('status-text-content');
const hintText       = document.getElementById('hint-text-content');

const mapTouchBtn    = document.getElementById('map-touch-btn');

// ----- ОТРИСОВКА -----
function updateUI() {
    state.food = Math.max(0, Math.min(100, state.food));
    state.fuel = Math.max(0, Math.min(100, state.fuel));

    foodBar.style.width = `${state.food}%`;
    fuelBar.style.width = `${state.fuel}%`;
    fuelContainer.style.opacity = state.fuel > 0 ? '1' : '0.3';

    partyPanel.innerHTML = '';
    state.party.forEach(member => {
        if (!member.active) return;
        member.hp   = Math.max(0, Math.min(100, member.hp));
        member.mood = Math.max(0, Math.min(100, member.mood));

        const memberDiv = document.createElement('div');
        memberDiv.className = 'member active';
        memberDiv.innerHTML = `
            <div class="member-name">${member.icon} ${member.name}</div>
            <div class="stat-row"><span title="Здоровье">❤️</span><div class="stat-bar-container"><div class="bar-fill" style="background-color: var(--hp-color); width: ${member.hp}%;"></div></div></div>
            <div class="stat-row"><span title="Настроение">😊</span><div class="stat-bar-container"><div class="bar-fill" style="background-color: var(--mood-color); width: ${member.mood}%;"></div></div></div>
        `;
        partyPanel.appendChild(memberDiv);
    });
}

function renderChoices() {
    choicesArea.innerHTML = '';
    currentChoices.forEach((choice, index) => {
        const choiceEl = document.createElement('div');
        choiceEl.className = 'choice-row' + (index === selectedChoiceIndex ? ' selected' : '');
        choiceEl.innerText = choice.action;

        // Тач/мышь: выбор пункта прямым кликом
        choiceEl.addEventListener('click', (e) => {
            e.stopPropagation();
            if (isAnimating) return;
            if (closeAnyOverlay()) return;

            selectedChoiceIndex = index;
            renderChoices();

            // Небольшая задержка перед применением — эффект «нажатия»
            setTimeout(() => {
                applyChoice();
            }, 100);
        });

        choicesArea.appendChild(choiceEl);
    });
}

function generateStatusText() {
    let txt = `ЗАПАСЫ:\n🍗 Еда: ${state.food}%\n`;
    if (state.fuel > 0) txt += `⛽ Бензин: ${state.fuel}%\n`;

    const transportName = state.transport === 0
        ? 'Пешком'
        : (state.transport === 1 ? 'Велосипед' : 'УАЗ Буханка');
    txt += `🚙 Транспорт: ${transportName}\n\nОТРЯД:\n`;

    state.party.forEach(m => {
        if (m.active) {
            txt += `- ${m.name}: Здоровье ${m.hp}%, Мораль ${m.mood}%\n`;
        }
    });
    return txt;
}

function resetGame() {
    state.food = 100;
    state.fuel = 0;
    state.transport = 0;
    state.party.forEach(m => {
        m.hp = 100;
        m.mood = 100;
        if (m.id !== 'hero') m.active = false;
    });
}

function loadEvent(eventId) {
    const hero = state.party.find(m => m.id === 'hero');
    if ((state.food <= 0 || hero.hp <= 0) && eventId !== 'start') {
        eventId = 'game_over';
    }

    const event = eventsData.find(e => e.id === eventId);
    if (!event) return;

    state.currentEventId = event.id;
    textArea.innerText = event.text;
    currentChoices = event.choices;
    currentHint = event.hint || 'Подсказки нет.';
    selectedChoiceIndex = 0;

    if (event.unlock_friend)   state.party.find(m => m.id === event.unlock_friend).active = true;
    if (event.unlock_friend_2) state.party.find(m => m.id === event.unlock_friend_2).active = true;
    if (event.fuel_mod)        state.fuel += event.fuel_mod;

    updateUI();
    renderChoices();
}

// ----- АНИМАЦИЯ (пиксель-арт спрайты) -----
// Каждый спрайт-шит — горизонтальная полоса кадров frameWidth x frameHeight.
// Смена кадра = сдвиг background-position по X. На ESP32 эквивалент —
// tft.pushImage(x, y, frameWidth, frameHeight, &frames[i * frameWidth * frameHeight]).
// Длительность перехода (1500 мс) одинаковая для всех — анимация просто
// «крутится» пока идёт затемнение между событиями.
const TRANSITION_MS = 1500;

// Какой спрайт показать для выбора. Приоритет:
//   1) choice.sprite                — точечное переопределение
//   2) state.currentEvent.sprite    — общий спрайт сцены (если задан)
//   3) transport_default[transport] — дефолт по текущему транспорту
function pickSprite(choice) {
    if (choice && choice.sprite) return choice.sprite;
    const event = eventsData.find(e => e.id === state.currentEventId);
    if (event && event.sprite) return event.sprite;
    return spritesData.transport_default[state.transport]
        || spritesData.transport_default[0];
}

function applySpriteFrame(name, frame) {
    const def = spritesData.sprites[name];
    if (!def) {
        spriteEl.style.display = 'none';
        return;
    }
    const w = def.frameWidth  * SPRITE_SCALE;
    const h = def.frameHeight * SPRITE_SCALE;
    const sheetW = def.frameWidth * def.frames * SPRITE_SCALE;

    spriteEl.style.display        = 'block';
    spriteEl.style.width          = `${w}px`;
    spriteEl.style.height         = `${h}px`;
    spriteEl.style.backgroundImage = `url("${def.src}")`;
    spriteEl.style.backgroundSize  = `${sheetW}px ${h}px`;
    spriteEl.style.backgroundPositionX = `-${(frame % def.frames) * w}px`;
    spriteEl.style.backgroundPositionY = '0px';
}

function playTransition(callback, choice) {
    isAnimating = true;
    animScreen.classList.add('show');

    const name = pickSprite(choice);
    const def  = spritesData.sprites[name];
    const fps  = (def && def.fps) ? def.fps : 4;
    let f = 0;

    applySpriteFrame(name, f);
    const interval = setInterval(() => {
        f = (f + 1) % (def ? def.frames : 1);
        applySpriteFrame(name, f);
    }, Math.round(1000 / fps));

    setTimeout(() => {
        clearInterval(interval);
        animScreen.classList.remove('show');
        isAnimating = false;
        callback();
    }, TRANSITION_MS);
}

function applyChoice() {
    const choice = currentChoices[selectedChoiceIndex];

    if (choice.next_id === 'start') {
        resetGame();
        loadEvent('start');
        return;
    }

    if (choice.transport_mod !== undefined) state.transport = choice.transport_mod;
    state.food += (choice.food_mod || 0);
    state.party.forEach(member => {
        if (member.active) {
            member.hp   += (choice.hp_mod || 0);
            member.mood += (choice.mood_mod || 0);
        }
    });

    playTransition(() => {
        loadEvent(choice.next_id);
    }, choice);
}

// ----- УПРАВЛЕНИЕ: короткие/длинные нажатия -----
// Эмуляция GPIO0 / GPIO35 на ESP32: короткое нажатие = листание/подтверждение,
// длинное (>600 мс) = вызов статуса/карты.
function setupButton(btnId, shortCb, longCb) {
    const btn = document.getElementById(btnId);
    let timer;
    let isLong = false;

    const start = (e) => {
        e.preventDefault();
        if (isAnimating) return;
        isLong = false;
        btn.style.transform = 'translateY(2px)';
        timer = setTimeout(() => {
            isLong = true;
            if (longCb) longCb();
        }, 600);
    };

    const end = (e) => {
        e.preventDefault();
        btn.style.transform = 'translateY(0)';
        clearTimeout(timer);
        if (!isLong && shortCb) shortCb();
    };

    btn.addEventListener('mousedown', start);
    btn.addEventListener('mouseup', end);
    btn.addEventListener('mouseleave', () => {
        clearTimeout(timer);
        btn.style.transform = 'translateY(0)';
    });
    btn.addEventListener('touchstart', start, { passive: false });
    btn.addEventListener('touchend', end, { passive: false });
}

// Единая точка закрытия любых оверлеев.
// Все обработчики (клавиатура, физические кнопки, тач) обязаны вызывать её
// первой и выходить, если она вернула true — иначе тап «провалится» в игру.
function closeAnyOverlay() {
    let closed = false;
    if (statusOverlay.classList.contains('show')) {
        statusOverlay.classList.remove('show');
        closed = true;
    }
    if (mapOverlay.classList.contains('show')) {
        mapOverlay.classList.remove('show');
        closed = true;
    }
    return closed;
}

// ----- ТАЧ-СОБЫТИЯ И ЗОНЫ -----
statusOverlay.addEventListener('click', closeAnyOverlay);
mapOverlay.addEventListener('click', closeAnyOverlay);

// Тап по панели отряда → статус / подсказка
partyPanel.addEventListener('click', (e) => {
    e.stopPropagation();
    if (closeAnyOverlay() || isAnimating) return;
    statusText.innerText = generateStatusText();
    hintText.innerText = '💡 ' + currentHint;
    statusOverlay.classList.add('show');
});

// Тап по иконке карты
mapTouchBtn.addEventListener('click', (e) => {
    e.stopPropagation();
    if (closeAnyOverlay() || isAnimating) return;
    mapOverlay.classList.add('show');
});

// ВЕРХНЯЯ кнопка (физическая): короткое — листать, длинное — Статус
setupButton('btn-top',
    () => {
        if (closeAnyOverlay()) return;
        if (currentChoices.length > 1) {
            selectedChoiceIndex = (selectedChoiceIndex + 1) % currentChoices.length;
            renderChoices();
        }
    },
    () => {
        if (closeAnyOverlay()) return;
        statusText.innerText = generateStatusText();
        hintText.innerText = '💡 ' + currentHint;
        statusOverlay.classList.add('show');
    }
);

// НИЖНЯЯ кнопка (физическая): короткое — ОК, длинное — Карта
setupButton('btn-bottom',
    () => {
        if (closeAnyOverlay()) return;
        applyChoice();
    },
    () => {
        if (closeAnyOverlay()) return;
        mapOverlay.classList.add('show');
    }
);

// Клавиатура (для отладки на десктопе)
window.addEventListener('keydown', (e) => {
    if (isAnimating) return;

    const validKeys = ['arrowup', '1', 'arrowdown', '2', 'enter', 'i', 'ш', 'm', 'ь', 'escape', ' '];
    if (validKeys.includes(e.key.toLowerCase())) {
        if (closeAnyOverlay()) return;
    }

    if (e.key === 'ArrowUp' || e.key === '1') {
        if (currentChoices.length > 1) {
            selectedChoiceIndex = (selectedChoiceIndex + 1) % currentChoices.length;
            renderChoices();
        }
    }
    if (e.key === 'ArrowDown' || e.key === '2' || e.key === 'Enter') {
        applyChoice();
    }
    if (e.key.toLowerCase() === 'i' || e.key.toLowerCase() === 'ш') {
        statusText.innerText = generateStatusText();
        hintText.innerText = '💡 ' + currentHint;
        statusOverlay.classList.add('show');
    }
    if (e.key.toLowerCase() === 'm' || e.key.toLowerCase() === 'ь') {
        mapOverlay.classList.add('show');
    }
});

// ----- ЗАПУСК -----
// Аналог: ESP32 при старте читает events.json и animations.json
// (Wi-Fi/SPIFFS) → JsonDocument, и только после этого запускает игровой цикл.
async function init() {
    try {
        const [eventsRes, spritesRes] = await Promise.all([
            fetch('./data/events.json'),
            fetch('./data/sprites.json')
        ]);
        if (!eventsRes.ok)  throw new Error(`events.json HTTP ${eventsRes.status}`);
        if (!spritesRes.ok) throw new Error(`sprites.json HTTP ${spritesRes.status}`);
        eventsData   = await eventsRes.json();
        spritesData  = await spritesRes.json();
    } catch (err) {
        textArea.innerText = 'Ошибка загрузки ресурсов: ' + err.message;
        return;
    }

    resetGame();
    loadEvent(state.currentEventId);
}

init();
