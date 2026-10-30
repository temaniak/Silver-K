# Первый этап оптимизации — 30 октября 2026

Полный NativeController оказался дороже H8. Профиль указал на записи с многократным линейным обходом 4154 immutable bindings. По опыту KS сначала устранены подтверждённые накладные расходы: поиск заменён бинарным поиском по фиксированному отсортированному объединению диапазонов. Исходный список bindings, verified flags, ёмкость, проверки перед записью, исключения, bus order и гостевые часы сохранены. Индекс добавляет около 64 KiB на состояние; выделений памяти в обработке нет.

KS изучался только для чтения. Его DSP catch-up loop не копировался: у K другие шаги и правила сохранения аудио. DSP/JIT, частоты, 128-clock границы, буферы, worker policy и приоритеты не менялись.

## Стоимость CPU на секунду звука

Метрика: user+system CPU всего процесса, включая DSP и тестовый host, делённый на длительность доставленного аудио. 100% означает одно логическое CPU. Это не изолированный CPU контроллера и не индикатор DAW.

Apple M1 Pro / MacBookPro18,3, 8 ядер; native arm64, macOS 27.2, AppleClang 21.0.0, Release `-O3 -DNDEBUG`, deployment target 12.0. Native rate 48611.111… Hz, block 256, dry fixtures. Серия: четыре сценария × три варианта × три повторения, по 8 секунд после 2 секунд прогрева; 36 окон, 288 секунд аудио. Порядок ABC/BCA/CAB уравнивает позиции вариантов. Наши сборки, другие DSP-прогоны, архивирование и profiler не работали одновременно с CPU-серией.

| Сценарий | H8 | Native до | Native после | Снижение к Native до |
|---|---:|---:|---:|---:|
| Idle | 39,01% | 107,11% | 42,32% | 60,49% |
| 8 удерживаемых нот | 39,85% | 119,93% | 43,12% | 64,04% |
| Меняющиеся ноты | 39,40% | 108,93% | 43,72% | 59,87% |
| Внешний clock burst | 40,11% | 113,98% | 44,21% | 61,21% |

Это медианы трёх запусков. Все 12 пар Native после/до улучшились. По медианам Native после ещё дороже H8 на 8,2–11,0%; из 12 отдельных сравнений с H8 ухудшились 11. Диапазоны Native после: idle 42,18–42,86%; held8 39,86–44,09%; notes 43,13–43,97%; clockburst 44,08–44,57%. У H8 clockburst диапазон 35,08–40,49%, поэтому малые эффекты требуют дополнительных повторов.

**Native до не удерживал pace:** все 12 его окон превысили audio duration +50 ms. Числа выше описывают стоимость CPU на доставленную секунду аудио при попытке paced playback; это не прошедшее matched realtime comparison. H8 и Native после прошли только эту грубую проверку длительности. Отдельный строгий deadline gate также не пройден.

## Дедлайны отдельно от CPU

Инструментированная серия: четыре сценария × три варианта, по 4 секунды после 2 секунд прогрева. Проверялись compute budget и завершение блока до абсолютного deadline.

| Вариант | Превышений compute budget | Поздних завершений | Максимальное опоздание |
|---|---:|---:|---:|
| H8 | 30 | 202 | 22,700 ms |
| Native до | 3008 | 3040 | 844,413 ms |
| Native после | 3 | 33 | 4,917 ms |

Ни один вариант не прошёл все дедлайны каждого из четырёх окон. Это короткий прогон обычного synthetic host без audio-priority модели и физического драйвера. Стабильное улучшение latency или готовность для DAW не заявляются. DSP dropped-frame counters нулевые; это отдельное свойство от host deadlines. Причина оставшихся опозданий не установлена.

## Точность

Оптимизированная версия заново прошла все 20 основных и 8 расширенных сценариев. Raw PCM/Flash/MIDI побайтно точны; RAM точна с прежним исключением только фактически записанного H8 call/IRQ context. Точны ordered IO clocks/data, pending IRQ и границы исполнения. На сторону: 2 722 216 PCM frames, 39 627 025 IO records, 17 976 237 границ. Допусков, выравнивания, подмены busy ответов или сокращения гостевой работы нет.

В 36 CPU-окнах совпали звуковые checksums, параметры и гостевые счётчики; Native до/после также имели одинаковый checksum всех 16 MiB RAM. Отдельные 22-секундные profile passages совпали по PCM/RAM/Flash checksums и счётчикам. Это дополнительные checksum gates, не замена raw differential proof. Native trace executable линкуется без H8; CPU executable содержит оба выбираемых backend для одинакового host. Доказательство связано общим замороженным исходником с разными диагностическими flags, а не идентичностью этих бинарников.

Focused index tests проверяют линейный oracle, overlap/adjacency/late binding, 24-bit wrap, failed verification, 8192 раздельных диапазона и полную ёмкость, atomic rejected writes и допустимые mapped writes. Release и unsuppressed ASan/UBSan прошли. Также прошли прежний DeviceState test, active-vector guards, bounded no-H8 standalone/reset/fault Release/SAN с нулём watched allocations, 1333 development guards и компиляция default Device consumer. Whole-engine sanitizer/DAW/Windows/hardware proof не заявляется.

## Воспроизведение и продолжение

Ветка `codex/kstation-native-cpu-20261030`, база `fda2229`. Игнорируемый `private/kstation-native-cpu-20261030/` содержит frozen sources/binaries и SHA256, `environment.json`, `cpu-final/summary.json`, `cpu-analysis.json`, `deadline-gate/summary.json`, `validation.json`, `index-tests/REPORT.md`, raw exact evidence ZIP и SHA manifests. Неудачные окна сохранены.

Plain CPU targets не содержат completion/trace hooks и per-block timers; отдельный deadline target содержит timers. Default restricted CPU runs использовали одинаковый существующий DSP MmuHelper fallback. Профили снимались с разрешением на inspection собственного процесса; их memory mapping отличался, и они исключены из CPU-статистики. CPU включает общий checksum/finite/RMS overhead host; фоновые системные процессы и частоты физических ядер не фиксировались.

`tools/kstationResearch/measure_controller_cpu.py` запускает чередующиеся окна. Его `passed` означает выполнение и функциональное сравнение; отдельно смотреть `paced_duration_failures` и `all_deadlines_passed`. Общий `passed=true` не является realtime admission. Использовать неизменные binary/runner из `final-runner/` и identities, новые output directories и те же параметры.

После правки функции записи перестали доминировать в профиле. Среди оставшихся активных стеков видны `ControllerForegroundRuntime::request`, `foregroundPhase`, timers и DSP transport. Следующий кандидат — повторная диспетчеризация foreground, с доказательством continuation/IRQ boundaries перед изменением. Она пока не оптимизировалась. Original остаётся default; main, KS и shared H8 не изменены.
