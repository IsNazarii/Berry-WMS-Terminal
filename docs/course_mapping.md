# Відповідність вимогам курсу ООП

Як вимоги лектора ([docs/source/course_requirements.md](source/course_requirements.md))
виконуються в коді. Документ оновлюється по ходу роботи.

## Загальні вимоги

| Вимога | Як виконано |
|---|---|
| Сучасна ООП-мова | C++20 (concepts, `std::variant`, `<=>`, `std::format`, designated initializers, ranges) |
| Одна програма, одна точка входу `main()` | `app/main.cpp` → `wms_terminal` |
| Git з регулярними змістовними комітами | малі кроки, кожен — окремий коміт; `git log` |
| Unit-тести (для «відмінно») | 284 тести GoogleTest у `tests/` (246 ядра + 38 infra на справжній SQLite), ядро пишеться тест-першим |
| Декларація використання ШІ | розділ у [README](../README.md#використання-ші), домовленості в `CLAUDE.md` |
| Аналіз конкурентів (для «відмінно») | ⏳ `docs/competitors.md` |

## Лабораторна 1 — базові ідеї ООП ✅

| Вимога | Мінімум | Є |
|---|---|---|
| Класи / типи | 9 | 24 класи ядра + допоміжні структури (`Weighing`, `Cargo`, `TruckDetails`, `PriceChange`, `Amendment`, `AmendmentRecord`, `WorkDayRecord`, `CropPrice`, `PaidReceipt`, `ToWarehouse`) |
| Поля | 15 | значно більше (напр., лише `Weighing` — 9, `WorkDay` — 10, `Truck` — 5) |
| Нетривіальні методи | 25 | значно більше (див. нижче) |
| Ієрархії успадкування | 2, одна ≥ 3 класи | A: `Transaction` → 5 класів; B: `IScaleDriver` → `SimulatedScaleDriver`; C: `JournalStore` → `SqliteJournalStore`, D: `SessionStore` → `SqliteSessionStore`, E: `Repository<T>` → `SqliteRepository<T>` (infra) |
| Поліморфізм | 3, ≥1 статичний, ≥1 динамічний | 7 випадків (таблиця нижче) |
| Інкапсуляція | доцільні рівні доступу | див. нижче |

### Класи ядра

`Quantity<Tag>` (`Money`, `Weight`), `Uuid`, `Supplier`, `Operator`, `Crop`, `CropKeys`, `Transaction`, `Receipt`,
`Payment`, `Allocation`, `AllocationReversal`, `StockShipment`, `OpenReceipt`, `Truck`, `Journal`, `DayPrices`, `WorkDay`, `SessionSummary`,
`IScaleDriver`, `SimulatedScaleDriver`, `StabilityDetector`, `JournalStore`, `SessionStore`, `Repository<T>` + infra-класи
`sqlite::Database`, `sqlite::Statement`, `sqlite::ScopedTransaction`, `sqlite::Error`, `SqliteJournalStore`, `SqliteSessionStore`, `SqliteRepository<T>` (+ функція `backupDatabase`) + UI-класи `Terminal`, `SupplierListModel`,
`ScaleMonitor`. Детально — [domain.md](domain.md).

### Ієрархії

```mermaid
classDiagram
    Transaction <|-- Receipt
    Transaction <|-- Payment
    Transaction <|-- Allocation
    Transaction <|-- AllocationReversal
    Transaction <|-- StockShipment
    IScaleDriver <|-- SimulatedScaleDriver
    IScaleDriver <|.. SerialScaleDriver : заплановано (infra, ІЕ-04)
    JournalStore <|-- SqliteJournalStore
    SessionStore <|-- SqliteSessionStore
    Repository~T~ <|-- SqliteRepository~T~
```

### Поліморфізм

| # | Вид | Де |
|---|---|---|
| 1 | Динамічний (віртуальні методи) | `Transaction::kind/balanceEffect/validate/describe` у 5 нащадках; `Journal::historyOf` віддає `const Transaction*` — Каса показує чеки й виплати однією колонкою сум через `balanceEffect()` |
| 2 | Динамічний (інтерфейс) | `IScaleDriver` — `ScaleMonitor` працює з будь-яким драйвером; `JournalStore`/`SessionStore` — ядро не знає, що дані лежать у SQLite |
| 3 | Статичний (шаблон з тегом) | `Quantity<Tag>`: `Money` і `Weight` — різні типи |
| 4 | Статичний (`std::variant` + `std::visit`) | `Destination = variant<TruckId, ToWarehouse>` в `Allocation::describe`, `Journal::loadOf/stockOf` |
| 5 | Статичний (перевантаження) | `Journal::add(Receipt / Payment / Allocation / AllocationReversal / StockShipment)`; `unallocatedOf`, `boxesOf`, `validateAllocation` для `Receipt` і `OpenReceipt`; `WorkDay::setPrice/addTruck/…` з `Amendment` і без |
| 6 | Статичний (concept) | `WeighedReceipt` — шаблонні `Allocation::wholeWeighing/boxesOf`, `Journal::checkAllocation` приймають `Receipt` і `OpenReceipt` |
| 7 | Статичний (шаблон класу) + динамічний | `Repository<T>`: один інтерфейс довідника для `Supplier`, `Operator` і `Crop`; `SqliteRepository<T>` бере таблицю й колонки зі спеціалізації `Table<T>` |

### Приклади нетривіальних методів

`Weighing::net/amount/validate`, `Receipt::total/cargoOf/amount/validate/describe`,
`OpenReceipt::add/cancel/crops/setPrice/validateClose/close`, `Allocation::wholeWeighing/boxesOf`,
`Journal::balanceOf/historyOf/loadOf/stockOf/unallocatedOf/destinationOf/allocationOf/reversalOf/waitingForAllocation/validateAllocation/validateCancel/validateReversal/validateShipment/shipmentOf`,
`Truck::depart/carryOver/boxesLeft/isOverloaded/fillPercent`, `DayPrices::priceAt`,
`WorkDay::openReceipt/addWeighing/validateCancel/cancelWeighing/nextDestination/autoAllocation/closeReceipt/validatePay/payReceipt/validateClose/close/repeatTruck/adoptTruck/trucksForNextSession`,
`Uuid::generateV7/parse/toString/timestamp`, `StabilityDetector::push/isStable/stableWeight`,
`Supplier::matches`, `validateNewSupplier/nextSupplierId`, `validateNewCrop/cropCodeFor/validateRemoveCrop`,
`CropKeys::use/fill`, `parseMoney/parseWeight`, `foldCase/squeezeSpaces`.

### Інкапсуляція

- Усі поля класів `private`; назовні — лише `const`-геттери з `[[nodiscard]]`.
- Конструктор `Quantity` з числа приватний — значення лише через іменовані фабрики.
- `Transaction`: конструктор і копіювання `protected` — неможливий slicing; нащадки `final`.
- `Operator`: UI має лише `checkPin`; PIN віддає тільки `pinForStorage()` — для запису в базу.
- `Truck`: статус змінюється лише через `depart()` / `carryOver()` з перевіркою переходу.
- `Journal`: лише додавання; жодного методу видалення чи зміни запису.
- `WorkDay`: закрита сесія змінюється тільки перевантаженнями з `Amendment`.

## Лабораторна 2 — GUI ✅

| Вимога | Мінімум | Стан |
|---|---|---|
| Логіка відділена від GUI | так | ядро без Qt; `Terminal` — тонкий міст, правила лише в `core/` ([ui.md](ui.md)) |
| Вікна / екрани | 4 | Вхід, Сесія, Прийом, Розподіл, Склад, Каса, пошук і додавання постачальника (+ діалоги «Додати фуру…», «Додати культуру…», прибрати культуру, закриття сесії, «Оплатити…», «Видати чек…», підтвердження виплати) |
| Елементи керування | 20 | > 20: `Button`, `TextField`, `ComboBox`, `RadioButton`, `TabButton`, `ToolButton`, `Menu`, `Dialog`… |
| Контейнер | 1 | `ListView` (постачальники, відкриті чеки, рядки чека, чеки до розподілу, фури/склад), `ListView` (борги, історія розрахунків), `Repeater` (ціни, фури, культури із залишком, залишок складу) |
| Обробники подій | 10 | > 30 (`onClicked`, `onAccepted`, `Shortcut` F2 / F5–F9 / F11, `Connections`…) |

Лабораторну закрито: шість екранів у власному стилі (`BerryWms.Style`), оболонка з вкладками й станом точки,
порожні стани; стан термінала й довідники (постачальники, оператори, культури) зберігаються в SQLite.
Як виглядає — знімки в [README](../README.md#як-це-виглядає). Подальші зміни інтерфейсу — точкові.

## Лабораторна 3 — зовнішня бібліотека ⏳

Друк чека постачальнику на звичайний принтер, один чек на сторінку A4 (рішення 30.10). Зовнішня бібліотека —
**nayuki qrcodegen**: кодує UUID чека в QR (рівень корекції, маска); модулі QR малює термінал. Сам чек верстає
й друкує Qt (`QPrinter` + `QPainter`) — Qt уже був у курсі, тому лабу він не закриває, а лише несе друк.
Звіт-ретроспектива — `docs/lab3_report.md`, список бібліотек — у README.
