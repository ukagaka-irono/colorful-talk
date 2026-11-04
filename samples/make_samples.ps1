# 確認用のサンプル WAV を samples/ に作る（先に Release ビルドしておくこと）
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ctalk = Join-Path $root 'build\Release\ctalk.exe'
$out = $PSScriptRoot

$items = @(
    @{ name = '01_hello';      text = "こんにちわ、せか'い。" },
    @{ name = '02_weather';    text = "きょ'うわ/い'い/てんきです。" },
    @{ name = '03_question';   text = "ほんとうに/そ'うですか？" },
    @{ name = '04_hashi_chopsticks'; text = "は'し。" },
    @{ name = '05_hashi_bridge';     text = "はし'/わたる。" },
    @{ name = '06_gojuon';     text = "あいうえお、かきくけこ、さしすせそ、たちつてと、なにぬねの、はひふへほ、まみむめも、やゆよ、らりるれろ、わをん。" },
    @{ name = '07_dakuon';     text = "がぎぐげご、ざじずぜぞ、だぢづでど、ばびぶべぼ、ぱぴぷぺぽ。" },
    @{ name = '08_youon';      text = "きゃきゅきょ、しゃしゅしょ、ちゃちゅちょ、にゃにゅにょ、ひゃひゅひょ、りゃりゅりょ。" },
    @{ name = '09_special';    text = "らーめん、きって、さんぽ、ふぁいる、ぱーてぃー。" },
    @{ name = '10_long';       text = "わたしわ、にほんごを/はなしま'す。むりょうで/つかえる/おんせいごうせいです。" }
)
$voices = @('f1', 'm1', 'child', 'robot')

foreach ($it in $items) {
    & $ctalk -o (Join-Path $out "$($it.name).wav") $it.text
}
foreach ($v in $voices) {
    & $ctalk -v $v -o (Join-Path $out "voice_$v.wav") "こんにちわ、からふる/とーくです。"
}
Write-Host "samples written to $out"
