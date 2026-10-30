# Оптимизация foreground — 30 октября 2026

Предел текущего этапа выполнен: во всех четырёх сценариях медиана CPU Native не выше H8 более чем на 3% **относительно H8**. Звук и гостевой тайминг сохранены в полной проверяемой матрице. Строгий host deadline gate всё ещё не пройден; готовность для DAW из этих результатов не следует. `Original` остаётся default, `Native` выбирается явно.

## Что изменено

По опыту KS сначала устранена подтверждённая профилем повторная работа. Root хранит owner foreground и продолжает известный активный UI/MIDI owner без повторного обхода цепочки. Неизвестный owner использует прежний маршрут. Предикаты foreground/UI повторяют условия полных getters и избегают создания пустых запросов. Результаты запросов не сохраняются через routing, IRQ или tick.

Private entry выбранного foreground сокращает повторные проверки только после доказанного Root admission. Public entry сохраняет прежние binding, active, mode и IRQ guards; unsupported случаи используют его. Нулевой результат повторяется через канонический admission только для проверенных pre-IO ожиданий и внешнего UnsupportedMode, внутри той же foregroundPhase; общий нулевой результат не повторяется. Лимит 64 попыток сохранён. Feedback leaf выполняет прежний блок при отсутствии более приоритетных children; trace проверяет эти предпосылки и совпадение предикатов.

Timer fast path применим только при отсутствии compare/zero события в интервале; остальная логика сохранена. Маска допустимых каналов пересчитывается при каждой записи TSTR/TMDR/TCR. HDI prepared step используется после проверки стоимости только для arbitration 0; arbitration 1 после bus callback заново использует проверяемый путь. Busy/poll операции и короткие бюджеты сохранены. Voice feedback credits вычисляются точными константами для подходящих RAM полей и прежней формулой для остальных; результат не кэшируется между arbitration.

Прямое чтение low RAM включается только приватным Native wrapper при активном overlay. Attach и разрешённый reset инвалидируют указатель; P8 обновляет его только для авторизованной прикреплённой шины. Указатель читает живую RAM. ROM, custom/generic bus, остальные адреса, записи, alias guards, побайтовые clocks/history и портовые traces сохраняют прежние пути. Владение предполагает прежний последовательный доступ, новой синхронизации нет.

KS использован только для чтения. Main/KS/shared H8, DSP/JIT, guest clocks, 128-clock границы, частоты, буферы, worker policy и приоритеты не изменялись. Нормальная обработка не получила новых allocations, locks, blocking IO или sleeps.

## CPU текущего этапа

Метрика прежняя: user+system CPU всего процесса, включая DSP, все его потоки и общий checksum/finite/RMS overhead host, делённый на длительность доставленного аудио. 100% соответствует одному логическому CPU. Это не изолированное измерение контроллера и не DAW meter.

Apple M1 Pro / MacBookPro18,3, 8 ядер; arm64, macOS 27.2, AppleClang 21, Release `-O3 -DNDEBUG`, deployment 12.0. Native rate 48611.111… Hz, block 256, dry fixtures. Один замороженный plain binary выбирает H8 или Native. Заранее объявленная серия: четыре сценария × два режима × восемь повторов, 8 секунд capture после 2 секунд прогрева, 64 окна / 512 секунд аудио; порядок AB/BA чередуется. Все восемь значений каждого режима включены в медиану. Наши сборки, тесты, архивирование и профилирование во время серии не работали.

| Сценарий | H8, % одного CPU | Native, % одного CPU | Native относительно H8 |
|---|---:|---:|---:|
| Idle | 39.191 | 39.707 | +1.314% |
| 8 удерживаемых нот | 39.386 | 39.273 | -0.289% |
| Меняющиеся ноты | 39.334 | 39.961 | +1.594% |
| Внешний clock burst | 41.215 | 41.921 | +1.711% |

Все 64 окна прошли функциональное сравнение и грубую paced-duration проверку (wall ≤ audio +50 ms); DSP dropped frames — ноль. Дополнительно совпали frames, H8/DSP instruction/cycle counters, HDI reads/writes, MIDI, PCM/Flash hashes, peak/RMS и параметры. Это checksum/counter gate; semantic RAM и ordered IO доказываются отдельно raw матрицей. Полный RAM hash стабилен внутри режима, но отличается между H8 и Native; один hash не устанавливает причину различия. Семантическая RAM проверена отдельной raw матрицей.

Разброс отдельных окон остаётся: H8/Native диапазоны idle 32.776–39.852 / 36.947–40.057%, held8 38.658–39.865 / 38.200–40.089%, notes 37.794–41.712 / 37.304–41.452%, clockburst 39.446–42.042 / 40.727–42.758%. Результат относится к медианам этой заранее объявленной серии, не к каждой паре или всем условиям. Фоновые процессы и частоты физических ядер не фиксировались. Во всех 64 логах одинаковый существующий MmuHelper fallback; отдельные профили исключены из CPU статистики.

Неудачные кандидаты и окна сохранены. В частности, предыдущий candidate 11 по заранее объявленному объединению восьми повторов провалил idle +4.499% и clockburst +8.374%; два clockburst Native окна также превысили paced duration. Его первые четыре повтора отдельно прошли порог, но не использованы вместо полного результата. Причина большого разброса не установлена.

## Host deadlines отдельно

Другой замороженный бинарник содержит per-block timers. Серия: четыре сценария × два режима × два повтора, 4 секунды после 2 секунд прогрева; 16 окон. Её CPU значения не используются в таблице выше.

| Режим | Compute overruns | Поздних блоков | Макс. опоздание | Окон без нарушений |
|---|---:|---:|---:|---:|
| H8 | 25 | 186 | 42.982 ms | 0/8 |
| Native | 16 | 118 | 2.923 ms | 1/8 |

Оба режима не прошли строгий gate. DSP dropped frames — ноль; coarse paced failures — ноль. Меньшее число опозданий Native в этой серии не является доказательством стабильного преимущества latency. Это обычный synthetic host без audio-priority модели и физического драйвера. Host deadlines отделены от точного гостевого тайминга.

## Точность и проверки

Финальный frozen candidate 12 заново прошёл все 20 основных и 8 расширенных сценариев: 2 722 216 PCM frames, 39 627 025 IO records и 17 976 237 границ на сторону. PCM/Flash/MIDI побайтно точны, semantic RAM совпадает; исключены только адреса фактических H8 call/IRQ writes с прежней проверкой отсутствия overlap с globals. Совпадают ordered IO clocks/data, pending IRQ и 128-clock boundaries. Нет допуска, сдвига, выравнивания или сокращения гостевой работы. Raw evidence сохранены lossless ZIP с SHA-verified roundtrip и manifests. Synthetic UI/TX fixtures не доказывают физический GPIO/UART.

Прошли predicate/selected-entry/feedback проверки на шести H8 fixtures, UI predicate fixtures, nullable voice routing, timer boundary/oracle и dynamic control tests, HDI task/runtime и arbitration callback cases, 1 000 200 credit-formula comparisons, low-read/Board/DeviceState, bounded no-H8 standalone, 1333 development guards и компиляция default Device consumer. Точные команды и границы каждого focused gate сохранены в отчётах evidence.

Low-read/Board/DeviceState/standalone и timer/HDI/credit focused sanitizer gates прошли unsuppressed ASan/UBSan. В Feedback/VoiceRefresh fixtures подавление signed overflow применяется только к H8 reference; Native инструментирован. UI sanitizer выявляет H8 signed overflow также на базовом HEAD, поэтому чистый UI sanitizer pass не заявляется. Известный baseline NoteDsp fixture failure не засчитан как успех. Standalone доказывает только ограниченный early boot/reset/fault, не весь движок. Универсальные no-throw, whole-engine sanitizer, DAW и hardware гарантии не заявляются.

## Воспроизведение

Ветка `codex/kstation-native-cpu-20261030`, база этапа `248264f`. Игнорируемый `private/kstation-foreground-cpu-20261030/` хранит `source-dispatch-12/identity.json`, `dispatch-12-binary/identity.json`, `cpu-direct-12/summary.json`, `cpu-analysis-12.json`, `deadline-direct-12/summary.json`, `deadline-analysis-12.json`, `exact-validation-12.json`, raw ZIP/manifests, focused reports, независимые TERRA reviews и `REPORT_RU.md`/`validation.json`. CPU executable SHA256: `e4922eb8ef45eace190cbc44017537ac6ccc3808b236c3ba8e5c3ee1b6e495bd`. Runner SHA256: `67feea980414a0662b50b752d828b31555d0b7ce439950920e7c94721a0bb0d7`.

С неизменными frozen binary/runner, ROM OS 2.1.01 и новыми output directories:

```sh
python3 private/kstation-native-cpu-20261030/final-runner/measure_controller_cpu.py "$ROM" "$CPU_OUT" \
  --variant h8 "$CPU_BINARY" original --variant native "$CPU_BINARY" native \
  --scenarios idle held8 notes clockburst --seconds 8 --warm-seconds 2 --repeats 8
python3 private/kstation-native-cpu-20261030/final-runner/measure_controller_cpu.py "$ROM" "$DEADLINE_OUT" \
  --variant h8 "$DEADLINE_BINARY" original --variant native "$DEADLINE_BINARY" native \
  --scenarios idle held8 notes clockburst --seconds 4 --warm-seconds 2 --repeats 2 --deadline-gate
```

Plain CPU target не содержит completion/trace hooks или per-block timers; native-only trace линкуется без H8. Исходники frozen и live проверены на совпадение. `summary.passed` означает функциональный успех; отдельно смотреть `paced_duration_failures` и `all_deadlines_passed`. Обычный tracked runner — `tools/kstationResearch/measure_controller_cpu.py`; frozen copy сохраняет точный использованный код вместе с импортом compare_completion.

Исторический отчёт первого этапа сохранён ниже.

---

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

После правки функции записи перестали доминировать в профиле. Среди оставшихся активных стеков видны `ControllerForegroundRuntime::request`, `foregroundPhase`, timers и DSP transport. Следующий кандидат — повторная диспетчеризация foreground, с доказательством continuation/IRQ boundaries перед изменением. Результат следующего этапа foreground приведён в начале этого документа. Original остаётся default; main, KS и shared H8 не изменены.
