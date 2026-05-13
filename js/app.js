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
 *     mood_mod, transport_mod, fuel_mod, next_id, result_title,
 *     result_text, result_scene, continue_text, unlock_friend[_2]);
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
//   spritesData.sprites[name] = { src, frameWidth, frameHeight, frames, fps, kind }
//   spritesData.transport_default = ['walk', 'bike', 'uaz']  // индекс = state.transport
// На ESP32 этот же манифест читает конвертер ассетов: каждый PNG переводится
// в массив RGB565 (или indexed+RLE) и линкуется в прошивку как PROGMEM-данные.
let spritesData = { sprites: {}, transport_default: [] };

// Закэшированные Image для отрисовки на канвасе. Заполняется в init().
// На ESP32 эквивалент — статические PROGMEM-блобы, привязанные по имени.
const spriteImages = {};

// Сцены из data/scenes.json. Каждая сцена — упорядоченный список слоёв
// (rect или sprite), который рендерится в канвас 204x115. На ESP32 этот
// же список превращается в последовательность tft.fillRect / tft.pushImage.
let scenesData = { scenes: {}, default_scene: null };

// Внутреннее разрешение канваса сцены. Соответствует ≈85% от 240x135
// (нативное разрешение T-Display). CSS-масштабирование на PWA ничего
// не меняет в логических координатах; на ESP32 эти числа идут 1:1.
const SCENE_W = 204;
const SCENE_H = 115;

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
let activeChoiceResult = null;
let scenePlaybackId = 0;

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
const sceneCanvas    = document.getElementById('scene-canvas');
const sceneCtx       = sceneCanvas.getContext('2d');
sceneCtx.imageSmoothingEnabled = false;
const resultCard     = document.getElementById('result-card');
const resultTitle    = document.getElementById('result-title-content');
const resultText     = document.getElementById('result-text-content');
const resultContinue = document.getElementById('result-continue-content');
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

function advanceToEvent(eventId) {
    if (eventId === 'start') resetGame();
    loadEvent(eventId);
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

// ----- АНИМАЦИЯ (сцены из пиксель-арт спрайтов) -----
// Сцена = упорядоченный список слоёв (data/scenes.json). Слой бывает двух
// типов:
//   - rect:   ctx.fillRect → tft.fillRect на ESP32
//   - sprite: ctx.drawImage(spriteSheet, srcX, 0, w, h, dx, dy, w, h)
//             → tft.pushImage(dx, dy, w, h, &frames[srcX_bytes]) на ESP32
// Параллакс реализован сдвигом world-offset = scrollSpeed × time_sec
// по модулю периода тайла; для каждой видимой копии — отдельный pushImage.
// Для конкретного спрайта можно остановить смену кадров через
// layer.animationSpeed = 0 или жестко выбрать кадр через freezeFrame.
// Длительность перехода между событиями фиксированная — анимация просто
// «крутится» нужное время, прежде чем мы загружаем следующее событие.
const TRANSITION_MS = 1500;

// Имя «героя» в шаблонах слоёв: scenes.json пишет sprite: "${hero}",
// а рендерер подставляет текущий транспорт игрока. На ESP32 та же
// подстановка делается перед циклом отрисовки слоёв.
const HERO_PLACEHOLDER = '${hero}';

function heroSprite() {
    return spritesData.transport_default[state.transport]
        || spritesData.transport_default[0]
        || 'walk';
}

// Какую сцену показать для выбора. Приоритет:
//   1) choice.scene                 — точечное переопределение
//      choice.scene === null        — явно не показывать обычную сцену
//   2) event.scene (текущее событие)
//   3) scenesData.default_scene
//   4) синтетическая сцена из одного спрайта (back-compat для choice.sprite /
//      event.sprite — на случай, если сцены ещё не описаны)
function pickScene(choice) {
    if (choice && Object.prototype.hasOwnProperty.call(choice, 'scene')) {
        if (choice.scene === null) return null;
        if (choice.scene && scenesData.scenes[choice.scene]) {
            return scenesData.scenes[choice.scene];
        }
    }

    const explicitName =
        (function () {
            const ev = eventsData.find(e => e.id === state.currentEventId);
            return ev && ev.scene;
        })() ||
        scenesData.default_scene;

    if (explicitName && scenesData.scenes[explicitName]) {
        return scenesData.scenes[explicitName];
    }

    // Back-compat: одна центрированная фигура героя на чёрном фоне.
    const spriteName =
        (choice && choice.sprite) ||
        (function () {
            const ev = eventsData.find(e => e.id === state.currentEventId);
            return ev && ev.sprite;
        })() ||
        heroSprite();
    return {
        background: '#000',
        layers: [
            { type: 'sprite', sprite: spriteName, x: SCENE_W / 2, y: SCENE_H - 5, anchor: 'center-bottom' }
        ]
    };
}

// Один слой со спрайтом. Возвращает [dx, dy] анкера → левый-верхний угол.
function anchorOffset(def, anchor) {
    switch (anchor) {
        case 'center-bottom': return [-def.frameWidth / 2, -def.frameHeight];
        case 'left-bottom':   return [0, -def.frameHeight];
        case 'top-left':
        default:              return [0, 0];
    }
}

function layerAnimationSpeed(layer) {
    if (typeof layer.animationSpeed === 'number') return layer.animationSpeed;
    return 1;
}

function drawSpriteLayer(layer, timeSec) {
    const name = layer.sprite === HERO_PLACEHOLDER ? heroSprite() : layer.sprite;
    const def  = spritesData.sprites[name];
    const img  = spriteImages[name];
    if (!def || !img || !img.complete || img.naturalWidth === 0) return;

    const fw = def.frameWidth;
    const fh = def.frameHeight;
    const frameTimeSec = timeSec * layerAnimationSpeed(layer);

    // Какой кадр анимации показать
    let frame = 0;
    if (typeof layer.freezeFrame === 'number') {
        frame = layer.freezeFrame % def.frames;
    } else if (def.frames > 1 && def.fps > 0) {
        frame = Math.floor(frameTimeSec * def.fps) % def.frames;
    }
    const srcX = frame * fw;

    const [ax, ay] = anchorOffset(def, layer.anchor || 'top-left');

    if (layer.tile) {
        // Бесконечная лента, едущая справа налево с заданной скоростью.
        const period = fw + (layer.spacing || 0);
        const baseY  = (layer.y != null ? layer.y : 0) + ay;
        // mod корректно работает для положительных и отрицательных значений
        let offset = ((layer.scrollSpeed || 0) * timeSec) % period;
        if (offset < 0) offset += period;
        // Начинаем с одной копии левее экрана, чтобы появление тайла было плавным
        for (let x = -offset; x < SCENE_W; x += period) {
            sceneCtx.drawImage(img, srcX, 0, fw, fh, Math.round(x), Math.round(baseY), fw, fh);
        }
    } else {
        const dx = Math.round((layer.x != null ? layer.x : 0) + ax);
        const dy = Math.round((layer.y != null ? layer.y : 0) + ay);
        sceneCtx.drawImage(img, srcX, 0, fw, fh, dx, dy, fw, fh);
    }
}

function drawRectLayer(layer) {
    sceneCtx.fillStyle = layer.color || '#000';
    sceneCtx.fillRect(
        Math.round(layer.x || 0),
        Math.round(layer.y || 0),
        Math.round(layer.w || 0),
        Math.round(layer.h || 0)
    );
}

function renderScene(scene, timeSec) {
    sceneCtx.fillStyle = scene.background || '#000';
    sceneCtx.fillRect(0, 0, SCENE_W, SCENE_H);
    for (const layer of scene.layers) {
        if (layer.type === 'rect')        drawRectLayer(layer);
        else if (layer.type === 'sprite') drawSpriteLayer(layer, timeSec);
    }
}

function stopScenePlayback() {
    if (!scenePlaybackId) return;
    cancelAnimationFrame(scenePlaybackId);
    scenePlaybackId = 0;
}

function startScenePlayback(scene, shouldContinue) {
    stopScenePlayback();

    const startMs = performance.now();
    const tick = (nowMs) => {
        const elapsedMs = nowMs - startMs;
        renderScene(scene, elapsedMs / 1000);
        if (shouldContinue && !shouldContinue(elapsedMs)) {
            scenePlaybackId = 0;
            return;
        }
        scenePlaybackId = requestAnimationFrame(tick);
    };

    scenePlaybackId = requestAnimationFrame(tick);
}

function showAnimationScreen(showResultCard, hasScene) {
    animScreen.classList.add('show');
    animScreen.classList.toggle('result-mode', showResultCard);
    sceneCanvas.classList.toggle('hidden', !hasScene);
    if (resultCard) resultCard.hidden = !showResultCard;
}

function hideAnimationScreen() {
    animScreen.classList.remove('show', 'result-mode');
    sceneCanvas.classList.remove('hidden');
    if (resultCard) resultCard.hidden = true;
}

function playTransition(callback, choice) {
    const scene = pickScene(choice);
    if (!scene) {
        callback();
        return;
    }

    isAnimating = true;
    showAnimationScreen(false, true);

    startScenePlayback(scene, (elapsedMs) => {
        if (elapsedMs < TRANSITION_MS) return true;

        hideAnimationScreen();
        isAnimating = false;
        callback();
        return false;
    });
}

function choiceHasResult(choice) {
    return Boolean(choice && (choice.result_text || choice.result_scene || choice.result_title));
}

function pickResultScene(choice) {
    if (!choice || !choice.result_scene) return null;
    return scenesData.scenes[choice.result_scene] || null;
}

function showChoiceResult(choice) {
    activeChoiceResult = {
        nextEventId: choice.next_id,
        pendingChoice: choice,
        title: choice.result_title || 'Последствия выбора',
        text: choice.result_text || '',
        continueText: choice.continue_text || 'Нажмите, чтобы продолжить'
    };

    resultTitle.innerText = activeChoiceResult.title;
    resultText.innerText = activeChoiceResult.text;
    resultContinue.innerText = activeChoiceResult.continueText;

    const scene = pickResultScene(choice);
    showAnimationScreen(true, Boolean(scene));

    if (scene) {
        startScenePlayback(scene, () => true);
    } else {
        stopScenePlayback();
    }
}

function continueChoiceResult() {
    if (!activeChoiceResult) return false;

    const nextEventId = activeChoiceResult.nextEventId;
    const pendingChoice = activeChoiceResult.pendingChoice;
    activeChoiceResult = null;
    stopScenePlayback();
    hideAnimationScreen();

    if (pendingChoice) {
        playTransition(() => {
            advanceToEvent(nextEventId);
        }, pendingChoice);
        return true;
    }

    advanceToEvent(nextEventId);
    return true;
}

function applyChoice() {
    const choice = currentChoices[selectedChoiceIndex];

    if (activeChoiceResult) {
        continueChoiceResult();
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

    if (choiceHasResult(choice)) {
        showChoiceResult(choice);
        return;
    }

    playTransition(() => {
        advanceToEvent(choice.next_id);
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
        if (activeChoiceResult) return;
        timer = setTimeout(() => {
            isLong = true;
            if (longCb) longCb();
        }, 600);
    };

    const end = (e) => {
        e.preventDefault();
        btn.style.transform = 'translateY(0)';
        clearTimeout(timer);
        if (activeChoiceResult) {
            continueChoiceResult();
            return;
        }
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
animScreen.addEventListener('click', (e) => {
    e.stopPropagation();
    continueChoiceResult();
});

// Тап по панели отряда → статус / подсказка
partyPanel.addEventListener('click', (e) => {
    e.stopPropagation();
    if (activeChoiceResult || closeAnyOverlay() || isAnimating) return;
    statusText.innerText = generateStatusText();
    hintText.innerText = '💡 ' + currentHint;
    statusOverlay.classList.add('show');
});

// Тап по иконке карты
mapTouchBtn.addEventListener('click', (e) => {
    e.stopPropagation();
    if (activeChoiceResult || closeAnyOverlay() || isAnimating) return;
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
    if (activeChoiceResult) {
        if (['ArrowUp', 'ArrowDown', '1', '2', 'Enter', ' '].includes(e.key)) {
            continueChoiceResult();
        }
        return;
    }

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
// Аналог: ESP32 при старте читает events.json / sprites.json / scenes.json
// (Wi-Fi/SPIFFS) → JsonDocument, прелоадит спрайты в PROGMEM, и только
// после этого запускает игровой цикл.
function preloadSpriteImages() {
    const promises = [];
    for (const [name, def] of Object.entries(spritesData.sprites)) {
        const img = new Image();
        img.src  = def.src;
        spriteImages[name] = img;
        // Дожидаемся декодирования, чтобы первая сцена не моргала пустотой.
        // При ошибке (нет файла) — продолжаем: рендерер просто пропустит слой.
        promises.push(
            img.decode ? img.decode().catch(() => {}) :
            new Promise(res => { img.onload = img.onerror = res; })
        );
    }
    return Promise.all(promises);
}

async function init() {
    try {
        const [eventsRes, spritesRes, scenesRes] = await Promise.all([
            fetch('./data/events.json'),
            fetch('./data/sprites.json'),
            fetch('./data/scenes.json')
        ]);
        if (!eventsRes.ok)  throw new Error(`events.json HTTP ${eventsRes.status}`);
        if (!spritesRes.ok) throw new Error(`sprites.json HTTP ${spritesRes.status}`);
        if (!scenesRes.ok)  throw new Error(`scenes.json HTTP ${scenesRes.status}`);
        eventsData  = await eventsRes.json();
        spritesData = await spritesRes.json();
        scenesData  = await scenesRes.json();
    } catch (err) {
        textArea.innerText = 'Ошибка загрузки ресурсов: ' + err.message;
        return;
    }

    await preloadSpriteImages();

    resetGame();
    loadEvent(state.currentEventId);
}

init();
