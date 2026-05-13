# Генератор плейсхолдер-спрайтов для прототипа "Дорога на восток".
#
# Назначение: создать рабочие PNG-ассеты, пока художественный пиксель-арт
# не нарисован. Архитектура (имена кадров, размеры, путь) совпадает с тем,
# что ждёт data/sprites.json и js/app.js — реальные спрайты подменяются
# поверх по тем же именам файлов.
#
# Запуск (из корня репо):
#   pwsh ./scripts/generate-placeholder-sprites.ps1
#
# Зависимости: System.Drawing (есть в PowerShell на Windows).

Add-Type -AssemblyName System.Drawing

$RepoRoot = Split-Path -Parent $PSScriptRoot
$OutDir   = Join-Path $RepoRoot 'assets\sprites'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# --- Палитра. Символ → HTML-цвет (или $null для прозрачного пикселя). ---
# Хэш-таблицы PowerShell регистр-нечувствительны, поэтому строим словарь
# через case-sensitive Dictionary<string,string>, чтобы 'G' и 'g' были разными.
$Palette = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([System.StringComparer]::Ordinal)
$Palette['.'] = $null         # transparent
$Palette['G'] = '#2ecc71'     # трава светлая
$Palette['g'] = '#27ae60'     # трава тёмная
$Palette['S'] = '#f1c40f'     # кожа / жёлтый акцент / фара
$Palette['H'] = '#1a1a1a'     # волосы / контур / резина
$Palette['T'] = '#e74c3c'     # одежда (красная)
$Palette['t'] = '#3498db'     # одежда (синяя)
$Palette['P'] = '#34495e'     # штаны / тёмный кузов
$Palette['B'] = '#95a5a6'     # металл / рама / бампер
$Palette['C'] = '#27ae60'     # кузов УАЗ
$Palette['c'] = '#1e8449'     # кузов УАЗ (тень)
$Palette['W'] = '#ecf0f1'     # стекло / белый
$Palette['D'] = '#555555'     # тёмно-серый / диск
$Palette['K'] = '#000000'     # чёрный
$Palette['M'] = '#4a4a6a'     # гора (основной)
$Palette['m'] = '#2a2a4a'     # гора (тень)
$Palette['J'] = '#1e6e2e'     # листва тёмная
$Palette['j'] = '#3aa856'     # листва светлая
$Palette['N'] = '#6f4e37'     # ствол дерева
$Palette['n'] = '#8b6a4a'     # ствол (светлый)
$Palette['F'] = '#ff6b35'     # пламя (тёплый)
$Palette['f'] = '#ffd166'     # пламя (горячий)
$Palette['A'] = '#7f8c8d'     # крыша гаража
$Palette['a'] = '#566970'     # крыша (тень)
$Palette['R'] = '#3a3a3a'     # дорога / асфальт
$Palette['r'] = '#1f1f1f'     # дорога (тень)
$Palette['Q'] = '#a3865d'     # стена гаража
$Palette['q'] = '#6e5a3d'     # стена гаража (тень)

function Save-SpriteSheet {
    param(
        [string]   $Path,
        [string[]] $Pixels,    # массив строк, каждая = одна строка пикселей
        [int]      $TargetW,   # требуемая ширина (обрежется/допадится точечками)
        [int]      $TargetH    # требуемая высота
    )
    # Приводим к точному размеру: лишние символы по краю — отрезаем, недостаток — дополняем '.'
    $Pixels = $Pixels | ForEach-Object {
        if ($_.Length -gt $TargetW) { $_.Substring(0, $TargetW) }
        else { $_.PadRight($TargetW, '.') }
    }
    if ($Pixels.Count -gt $TargetH) { $Pixels = $Pixels[0..($TargetH - 1)] }
    while ($Pixels.Count -lt $TargetH) { $Pixels += ('.' * $TargetW) }

    $h = $Pixels.Count
    $w = $Pixels[0].Length

    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt $h; $y++) {
            $row = $Pixels[$y]
            for ($x = 0; $x -lt $w; $x++) {
                $ch = [string]$row[$x]
                if (-not $Palette.ContainsKey($ch)) { continue }
                $hex = $Palette[$ch]
                if ($null -eq $hex) { continue }   # transparent
                $color = [System.Drawing.ColorTranslator]::FromHtml($hex)
                $bmp.SetPixel($x, $y, $color)
            }
        }
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $Path ($w x $h)"
    } finally {
        $bmp.Dispose()
    }
}

# ============================================================================
# WALK — 2 кадра по 24x32. Sprite-sheet 48x32.
# Левая половина: ноги вместе. Правая: шагающая поза.
# ============================================================================
$walk = @(
    '............................................... ',
    '............................................... ',
    '............................................... ',
    '............................................... ',
    '............................................... ',
    '............................................... ',
    '..........SSS......................SSS.......... ',
    '.........HHHHH....................HHHHH......... ',
    '.........HSSSH....................HSSSH......... ',
    '.........HSSSH....................HSSSH......... ',
    '..........SSS......................SSS.......... ',
    '..........TTT......................TTT.......... ',
    '.........TTTTT....................TTTTT......... ',
    '........TTTTTTT..................TTTTTTT........ ',
    '.......TTTTTTTTT................TTTTTTTTT....... ',
    '........TTTTTTT..................TTTTTTT........ ',
    '.........TTTTT....................TTTTT......... ',
    '..........PPP......................PPP.......... ',
    '..........PPP......................PPP.......... ',
    '..........PPP......................PPP.......... ',
    '.........PP.PP....................PPP.PP........ ',
    '.........PP.PP...................PPP...PP....... ',
    '.........PP.PP..................PPP.....PP...... ',
    '.........PP.PP.................PPP.......PP..... ',
    '.........HH.HH................HHH.........HH.... ',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG ',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg ',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG ',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg ',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG ',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg ',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG '
) | ForEach-Object { $_.TrimEnd(' ') }   # хвостовые пробелы — для удобства правки

Save-SpriteSheet -Path (Join-Path $OutDir 'walk.png') -Pixels $walk -TargetW 48 -TargetH 25

# ============================================================================
# BIKE — 2 кадра по 32x32. Sprite-sheet 64x32.
# Кадры отличаются положением спиц.
# ============================================================================
$bike = @(
    '................................................................',
    '................................................................',
    '...........SSS.................................SSS.............',
    '..........HHHHH...............................HHHHH............',
    '..........HSSSH...............................HSSSH............',
    '..........HSSSH...............................HSSSH............',
    '...........SSS.................................SSS.............',
    '...........ttt.................................ttt.............',
    '..........ttttt...............................ttttt............',
    '.........ttttttt.............................ttttttt...........',
    '..........ttttt...............................ttttt............',
    '...........ttt.................................ttt.............',
    '...........PPP.................................PPP.............',
    '..........PPPPP...............................PPPPP............',
    '.........BB...BB.............................BB...BB...........',
    '........BB..B..BB...........................BB..B..BB..........',
    '.......BB..BBB..BB.........................BB..BBB..BB.........',
    '......BB..BBBBB..BB.......................BB..BBBBB..BB........',
    '.....BB..BBBBBBB..BB.....................BB..BBBBBBB..BB.......',
    '....BBB.BBB...BBB.BBB...................BBB.BBB...BBB.BBB......',
    '...HHHHH.BBBBBBB.HHHHH.................HHHHH.BBBBBBB.HHHHH.....',
    '..HHDDDHH.BBBBB.HHDDDHH...............HHDDDHH.BBBBB.HHDDDHH....',
    '..HDDDDDH..BBB..HDDDDDH...............HDDDDDH..BBB..HDDDDDH....',
    '..HDDDDDH...B...HDDDDDH...............HDDDDDH...B...HDDDDDH....',
    '..HDDDDDH..BBB..HDDDDDH...............HDDDDDH..BBB..HDDDDDH....',
    '..HHDDDHH.BBBBB.HHDDDHH...............HHDDDHH.BBBBB.HHDDDHH....',
    '...HHHHH.BBBBBBB.HHHHH.................HHHHH.BBBBBBB.HHHHH.....',
    '....HHH.BBB...BBB.HHH...................HHH.BBB...BBB.HHH......',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'bike.png') -Pixels $bike -TargetW 64 -TargetH 28

# ============================================================================
# UAZ — 2 кадра по 48x32. Sprite-sheet 96x32.
# Кадры отличаются позицией спицы колеса.
# ============================================================================
$uaz = @(
    '................................................................................................',
    '................................................................................................',
    '................................................................................................',
    '................................................................................................',
    '......CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC................CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC..........',
    '.....CCWWWWWWWWWCCWWWWWWWWWCCCCCCCCCCCCC.............CCWWWWWWWWWCCWWWWWWWWWCCCCCCCCCCCCC.........',
    '....CCCWWWWWWWWWCCWWWWWWWWWCCCCCCCCCCCCCC...........CCCWWWWWWWWWCCWWWWWWWWWCCCCCCCCCCCCCC........',
    '...CCCCWWWWWWWWWCCWWWWWWWWWCCCCCCCCCCCCCCC.........CCCCWWWWWWWWWCCWWWWWWWWWCCCCCCCCCCCCCCC.......',
    '...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.........CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.......',
    '..CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC.......CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC......',
    '..CCccccccccccccccccccccccccccccccccccccccC.......CCccccccccccccccccccccccccccccccccccccccC......',
    '..CCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcC.......CCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcC......',
    '..CCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcC.......CCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcC......',
    '..CCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcC.......CCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcC......',
    '.SCCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcCS.....SCCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcCS.....',
    '.SCCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcCS.....SCCcCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCcCS.....',
    '.SBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBS.....SBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBS.....',
    '..BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.......BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB......',
    '...HHHHH...................HHHHH...................HHHHH...................HHHHH................',
    '..HHDDDHH.................HHDDDHH.................HHDDDHH.................HHDDDHH...............',
    '..HDDKDDH.................HDDKDDH.................HDDDDDH.................HDDDDDH...............',
    '..HDDDDDH.................HDDDDDH.................HDKKKDH.................HDKKKDH...............',
    '..HDDKDDH.................HDDKDDH.................HDDDDDH.................HDDDDDH...............',
    '..HHDDDHH.................HHDDDHH.................HHDDDHH.................HHDDDHH...............',
    '...HHHHH...................HHHHH...................HHHHH...................HHHHH................',
    '................................................................................................',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg',
    'gGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgG',
    'GgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGgGg'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'uaz.png') -Pixels $uaz -TargetW 96 -TargetH 26

# ============================================================================
# MOUNTAIN — 1 кадр 80x32. Дальний фон, без анимации, бесшовно тайлится.
# Крайние столбцы должны стыковаться, поэтому профиль гор «закольцован»:
# первая колонка повторяет последнюю.
# ============================================================================
$mountain = @(
    '................................................................................',
    '................................................................................',
    '................................................................................',
    '................................................................................',
    '.....................M........................M................................',
    '....................MMM......................MMM...............................',
    '....................MMM.........M............MMM................M..............',
    '...................MMMMM.......MMM..........MMMMM..............MMM.............',
    '..M...............MMMmMM......MMMMM........MMMmMMM.............MMMM............',
    '.MMM.............MMMmMMMM....MMMMMMM......MMMmMMMMM...........MMMmMM............',
    'MMMMM...........MMMmMMMMMM..MMMMmMMMM....MMMmMMMMMMM.........MMMmMMMM...........',
    'MMmMMM.........MMMmMMMMMMMMMMMmMMMMMMM..MMmMMMMMMMMMM.......MMmMMMMMMM..........',
    'MMmMMMM.......MMmMMMMMMMMMMMmMMMMMMMMMMMMmMMMMMMMMMMMM.....MmMMMMMMMMMM.........',
    'MMmMMMMM.....MMmMMMMMMMMMMMmMMMMMMMMMMMMmMMMMMMMMMMMMMMM.MMmMMMMMMMMMMMMM........',
    'MMmMMMMMM...MMmMMMMMMMMMMMmMMMMMMMMMMMMMmMMMMMMMMMMMMMMMMMmMMMMMMMMMMMMMMM.......',
    'MMmMMMMMMMMMMmMMMMMMMMMMMMmMMMMMMMMMMMMMmMMMMMMMMMMMMMMMMmMMMMMMMMMMMMMMMMMM.....',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm',
    'mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'mountain.png') -Pixels $mountain -TargetW 80 -TargetH 32

# ============================================================================
# TREE — 1 кадр 16x24. Объект среднего плана; передвигается параллаксом.
# Прозрачный фон, нижний ряд = землю не рисуем (её даёт сама сцена).
# ============================================================================
$tree = @(
    '.......jjj......',
    '......jJJJj.....',
    '.....jJJJJJj....',
    '....jJJJjJJJj...',
    '....jJJJJJJJj...',
    '...jJJJjJJJJJj..',
    '...jJJJJJJJJJj..',
    '...jJJJJJJJJJj..',
    '....jJJJJJJJj...',
    '....jJJJjJJJj...',
    '.....jJJJJJj....',
    '......jJJJj.....',
    '.......NNN......',
    '.......NNN......',
    '.......NnN......',
    '.......NNN......',
    '.......NnN......',
    '.......NNN......',
    '.......NnN......',
    '.......NNN......',
    '......NNNNN.....',
    '.....NNNnNNN....',
    '....NNNnNnNNN...',
    '...NNNnNnNnNNN..'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'tree.png') -Pixels $tree -TargetW 16 -TargetH 24

# ============================================================================
# BUSH — 1 кадр 12x10. Ближний план; быстрый параллакс.
# ============================================================================
$bush = @(
    '....jjjj....',
    '...jJjjJj...',
    '..jJJjjJJj..',
    '.jJJjjjjJJj.',
    'jJJjjJjjjJJj',
    'jJJjjjJjjJJj',
    'jJjjJjjjjjJj',
    '.jJjjjjjjJj.',
    '..jJjjjjJj..',
    '...jjJJjj...'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'bush.png') -Pixels $bush -TargetW 12 -TargetH 10

# ============================================================================
# GARAGE — 1 кадр 56x40. Простая хозпостройка с двускатной крышей
# и металлическими воротами.
# ============================================================================
$garage = @(
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '............................AA..........................',
    '...........................AAAA.........................',
    '..........................AAaAAA........................',
    '.........................AAaaAAAA.......................',
    '........................AAaaaaAAAA......................',
    '.......................AAaaaaaaAAAA.....................',
    '......................AAaaaaaaaaAAAA....................',
    '.....................AAaaaaaaaaaaAAAA...................',
    '....................AAaaaaaaaaaaaaAAAA..................',
    '...................AAaaaaaaaaaaaaaaAAAA.................',
    '..................AAaaaaaaaaaaaaaaaaAAAA................',
    '.................AAAAAAAAAAAAAAAAAAAAAAAA...............',
    '................QqqqqqqqqqqqqqqqqqqqqqqqqQ..............',
    '................QQQQQQQQQQQQQQQQQQQQQQQQQQQ.............',
    '................QHHHHHHHHHHHHHHHHHHHHHHHHHQ.............',
    '................QHDDDDDDDHHDDDDDDDHHDDDDDHQ.............',
    '................QHDKKKKKDHHDKKKKKDHHDKKKDHQ.............',
    '................QHDKKKKKDHHDKKKKKDHHDKKKDHQ.............',
    '................QHDKKKKKDHHDKKKKKDHHDKKKDHQ.............',
    '................QHDKKKKKDHHDKKKKKDHHDKKKDHQ.............',
    '................QHDKKKKKDHHDKKKKKDHHDKKKDHQ.............',
    '................QHDDDDDDDHHDDDDDDDHHDDDDDHQ.............',
    '................QHHHHHHHHHHHHHHHHHHHHHHHHHQ.............',
    '................QHHHHHHHHHHSSHHHHHHHHHHHHHQ.............',
    '................QHHHHHHHHHSSSSHHHHHHHHHHHHQ.............',
    '................QQQQQQQQQQQQQQQQQQQQQQQQQQQ.............',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................',
    '........................................................'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'garage.png') -Pixels $garage -TargetW 56 -TargetH 40

# ============================================================================
# FIRE — 2 кадра 16x16. Анимация языков пламени без перемещения.
# ============================================================================
$fire = @(
    '................................',
    '................................',
    '.......f...............f........',
    '......fFf.............fFf.......',
    '......fFf............ffFff......',
    '.....ffFff...........fFFFf......',
    '....fFFFFf..........ffFFFff.....',
    '....fFKFFf..........fFKFFFf.....',
    '...ffFKKFFf........ffFKKFFf.....',
    '...fFFKKFFf........fFFKKFFFf....',
    '...fFFKKFFFf.......fFFKKFFFf....',
    '....fFFFFFFf........fFFFFFf.....',
    '....fFFFFFFf........fFFFFFf.....',
    '.....fFFFFf..........fFFFf......',
    '......KKKK............KKKK......',
    '................................'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'fire.png') -Pixels $fire -TargetW 32 -TargetH 16

# ============================================================================
# SIT — 1 кадр 16x16. Сидящая фигура у костра.
# ============================================================================
$sit = @(
    '................',
    '.....SSSS.......',
    '....HHHHHH......',
    '....HSSSSH......',
    '....HSSSSH......',
    '.....SSSS.......',
    '....TTTTTT......',
    '...TTTTTTTT.....',
    '..TTTTTTTTTT....',
    '..TTTTTTTTTT....',
    '..PPTTTTTTPP....',
    '..PPPPTTPPPP....',
    '..PP.PPPP.PP....',
    '..HH.HHHH.HH....',
    '..HH.HHHH.HH....',
    '................'
)
Save-SpriteSheet -Path (Join-Path $OutDir 'sit.png') -Pixels $sit -TargetW 16 -TargetH 16

Write-Host ""
Write-Host "Done. Sprites written to $OutDir"
