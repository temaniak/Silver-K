# Нативный контроллер K-Station

Контроллер OS 2.1.01 перенесён в самостоятельные C++ службы. DSP56300 по-прежнему исполняет оригинальную DSP-прошивку. KS использован как образец разделения служб, общего DSP transport и планирования; исходники и ветка KS не изменялись.

`Hardware(rom, flash, ControllerMode::Native)` создаёт только `NativeController`. Обычный режим по умолчанию пока `Original`: включение в продукт и оптимизация CPU отделены от функционального переноса. Нативный backend не содержит H8 CPU, decoder, PC, CCR, register bank или гостевого стека. `ControllerTypes.h` хранит общие типы интерфейса; `H8` использует их aliases. Reference-only `core()` в нативном режиме недоступен.

Поддерживаемая прошивка: decoded OS 2.1.01, 49 728 байт, SHA256 `2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07`. ROM, DSP image, дисассемблирование и сырые результаты остаются в игнорируемом `private/` либо в пользовательском каталоге ROM. В репозитории нет ROM payload.

## Владение и время

- `ControllerBoard` владеет RAM, Flash, GPIO/LCD, UART и ADC; `ControllerTimers` моделирует таймеры.
- `ControllerInterruptRuntime` обслуживает Timer1/Timer3, SCI RX/TX/error, ADC и общий clock graph, включая входящий F8.
- Startup, foreground, MIDI, voices/parameters, arpeggiator, panel/menu, deferred callbacks, script playback и firmware receiver составлены из типизированных служб. Общие DSP/LCD/numeric owners передаются по ссылке; reentry в занятый owner запрещён.
- Каждая timed phase воспроизводит часы и момент наблюдаемого доступа. IRQ admission и первая фаза обработчика считаются одной исходной фазой; периферия обновляется один раз после них. `exec(128)` сохраняет исходное превышение бюджета последней фазой и прежнюю синхронизацию DSP 7:1.
- Реальный boot устанавливает overlay и только затем связывает physical aliases. Early receiver/F7C допускает low-only image и masked unbound IRQ. Reset пересоздаёт board и owners вместе вне обработки.

Данные, которые прошивка меняет после загрузки ROM в RAM, читаются живыми. В частности, C06C..C16C пересекается с keyboard queue, table pairs пересекаются с ADC, а menu selections находятся рядом с AF handler table. Активные векторы 28/38/52–54/60 защищены; unused vector bytes, изменяемые исходным packer, остаются доступны.

Сохранены особенности эталона: clock selector 0 означает Auto; unsigned division by zero даёт исходный нулевой quotient; DEC.B 0x80 учитывает исходный знак промежуточного результата; packer 525C делает исходные low-address stores. Исправление этих особенностей изменило бы поведение относительно H8.

## Проверка

`tools/kstationResearch/compare_native_completion.py` сравнивает с аутентифицированным архивом H8. `compare_native_extended.py` запускает свежий Original oracle и нативный бинарник для SysEx/NRPN/expression/program и synthetic panel fixtures. `uipanel/uimenu/uistore` явно публикуют keyboard queue words и rotary timer counts; физический GPIO ими не заявляется.

PCM, Flash и MIDI сравниваются побайтно. Во всех 16 MiB RAM исключаются только адреса с зарегистрированной фактической записью call/IRQ context. Для 20 архивных сценариев источник этих адресов — предыдущая H8/hybrid трасса integration6, которая побайтно совпала с Original, включая RAM и архитектурные slices. Расширенные сценарии используют свежий Original stack audit каждого прогона. Пересечение с живыми globals до FFFE91 приводит к провалу проверки. В port traces исключается только PC; в 128-clock boundaries — архитектурные PC/CCR/EXR/register/history поля. Часы, pending IRQ, порядок и значения IO остаются точными. Звукового или временного допуска, сдвига и выравнивания нет.

Основная матрица: 20 сценариев, 1 944 440 PCM frames, 26 754 085 IO events и 12 150 253 границы исполнения на сторону. Ещё пять свежих сценариев проверяют SysEx, граничные NRPN, expression и program stress. Три финальных UI-сценария проверяют десять keyboard handlers; дополнительный сохранённый вариант UI4 подтверждает одиннадцатый, 3E26 (compare). Parameter Menu дополнительно требует фактические входы 44DC/45AA/3C6E/3CA6/481A. Все 28 сценариев совпали. Они используют несколько замороженных research executables с одинаковыми 789 файлами K C++/headers; изменялся только стенд, а не контроллер. Сводные результаты и identities сохранены в игнорируемом `private/kstation-controller-completion-20261029/`.

UI fixture впервые подаёт клавиши во время capture. Явный synthetic level-ready TX IRQ компенсирует отсутствие начального TX interrupt при включении TIE на уже готовом передатчике в обеих существующих board-моделях. Он не изменяет указатели TX-очереди: dequeue и запись TDR выполняет прошивка. Diagnostic reads и их гостевые часы входят в сравнение обеих сторон. Это тест служб, а не подтверждение электрического поведения UART. Board API, как и Original, пока возвращает отпущенные физические клавиши, неподвижный encoder и фиксированные ADC inputs.

Направленные UI → script → MIDI/voice испытания включают реальные clock children IRQ28 и IRQ53/F8, ADC IRQ60, `7930/79A4/610E/83A2`. Худшая наблюдавшаяся запись контекста FFFE96 оставляет четыре байта над первым адресом после globals. Это границы проверенных трасс, а не формальное доказательство для всех повреждённых состояний.

`kstation_native_only_trace` собирает настоящие Hardware + NativeController + DSP без `kstationH8.cpp` и CPU include path. Проверка link/symbols дополняет сравнение звука: H8 fallback в таком бинарнике невозможен.

Опция `KSTATION_BUILD_CONTROLLER_SERVICE_CHECKS=ON` регистрирует отдельные service tests. Без внешнего ROM соответствующие CTest возвращают skip 77; executable запускается непосредственно с путём к ROM. `kstationNativeControllerStandalone_test` линкует только свой исходник, NativeController и SHA256, без H8 и DSP. Его fake ports доказывают ранний boot/reset/overlay и allocation-free fault latch, но не полный boot или звук; полный boot проверяется отдельным DSP-backed бинарником.

## Границы realtime и ошибок

Нормальные новые controller phases используют фиксированные owners/очереди; новых heap allocations, блокирующего IO, locks и sleeps в них нет. Constructor/resource binding и отдельные fault/admission проверки проверяются отдельно. Новый routing fault хранит указатель на постоянную строку и останавливает backend; он не вызывает H8 fallback.

Это не универсальная гарантия no-throw/no-allocation для всех повреждённых входов: неподдержанное стирание сектора Flash, busy HDI write, переполнение host MIDI output и immutable resource guards имеют явные fatal исключения. Host сообщает latched fault после возврата backend. Существующий DSP JIT не входит в утверждение об allocation-free controller phases. Не следует выдавать happy-path allocation counters за доказательство всех аварийных ветвей.

CPU benchmarks и профилирование в ходе завершения переноса не запускались. Финальный `K_RESEARCH_FUNCTIONAL_ONLY` также отключает служебные host block timers и отвергает paced/detailed modes; исторические отчёты сохраняют прежние incidental block wall fields, которые не использовались для оценки CPU. Производительность и paced realtime budget требуют отдельного следующего этапа; из точного функционального соответствия ускорение не следует. Прежняя неудача realtime gate на stage4 этими функциональными проверками не снимается.

## Отдельный этап CPU

Первое измерение и оптимизация полного NativeController выполнены 30 октября: см. [NATIVE_CONTROLLER_CPU.md](NATIVE_CONTROLLER_CPU.md). CPU на секунду звука снизился, но Native пока дороже H8 и realtime gate не пройден. Историческое отключение CPU выше относится к этапу завершения функционального переноса.
