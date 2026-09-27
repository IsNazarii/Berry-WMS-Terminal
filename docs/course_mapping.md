# Відповідність вимогам курсу ООП

Як вимоги лектора ([docs/source/course_requirements.md](source/course_requirements.md))
виконуються в коді. Документ оновлюється по ходу роботи.

## Загальні вимоги

| Вимога | Як виконано |
|---|---|
| Сучасна ООП-мова | C++20 (concepts, `std::variant`, `<=>`, `std::format`, designated initializers, ranges) |
| Одна програма, одна точка входу `main()` | `app/main.cpp` → `wms_terminal` |
| Git з регулярними змістовними комітами | малі кроки, кожен — окремий коміт; `git log` |
| Unit-тести (для «відмінно») | 164 тести GoogleTest у `tests/`, ядро пишеться тест-першим |
| Декларація використання ШІ | розділ у [README](../README.md#використання-ші), домовленості в `CLAUDE.md` |
| Аналіз конкурентів (для «відмінно») | ⏳ `docs/competitors.md` |

## Лабораторна 1 — базові ідеї ООП ✅

| Вимога | Мінімум | Є |
|---|---|---|
| Класи / типи | 9 | 19 класів ядра + допоміжні структури (`Weighing`, `Cargo`, `TruckDetails`, `PriceChange`, `Amendment`, `AmendmentRecord`, `ToWarehouse`) |
| Поля | 15 | значно більше (напр., лише `Weighing` — 9, `WorkDay` — 9, `Truck` — 5) |
| Нетривіальні методи | 25 | значно більше (див. нижче) |
| Ієрархії успадкування | 2, одна ≥ 3 класи | A: `Transaction` → 4 класи; B: `IScaleDriver` → `SimulatedScaleDriver` |
| Поліморфізм | 3, ≥1 статичний, ≥1 динамічний | 6+ випадків (таблиця нижче) |
| Інкапсуляція | доцільні рівні доступу | див. нижче |

### Класи ядра

`Quantity<Tag>` (`Money`, `Weight`), `Uuid`, `Supplier`, `Operator`, `Transaction`, `Receipt`,
`Payment`, `Allocation`, `StockShipment`, `OpenReceipt`, `Truck`, `Journal`, `DayPrices`, `WorkDay`,
`IScaleDriver`, `SimulatedScaleDriver`, `StabilityDetector` + UI-класи `Terminal`, `SupplierListModel`,
`ScaleMonitor`. Детально — [domain.md](domain.md).

### Ієрархії

```mermaid
classDiagram
    Transaction <|-- Receipt
    Transaction <|-- Payment
    Transaction <|-- Allocation
    Transaction <|-- StockShipment
    IScaleDriver <|-- SimulatedScaleDriver
    IScaleDriver <|.. SerialScaleDriver : заплановано (infra)
```

### Поліморфізм

| # | Вид | Де |
|---|---|---|
| 1 | Динамічний (віртуальні методи) | `Transaction::kind/balanceEffect/validate/describe` у 4 нащадках |
| 2 | Динамічний (інтерфейс) | `IScaleDriver` — `ScaleMonitor` працює з будь-яким драйвером |
| 3 | Статичний (шаблон з тегом) | `Quantity<Tag>`: `Money` і `Weight` — різні типи |
| 4 | Статичний (`std::variant` + `std::visit`) | `Destination = variant<TruckId, ToWarehouse>` в `Allocation::describe`, `Journal::loadOf/stockOf` |
| 5 | Статичний (перевантаження) | `Journal::add(Receipt / Payment / Allocation / StockShipment)`; `unallocatedOf`, `boxesOf`, `validateAllocation` для `Receipt` і `OpenReceipt`; `WorkDay::setPrice/addTruck/…` з `Amendment` і без |
| 6 | Статичний (concept) | `WeighedReceipt` — шаблонні `Allocation::wholeWeighing/boxesOf`, `Journal::checkAllocation` приймають `Receipt` і `OpenReceipt` |

### Приклади нетривіальних методів

`Weighing::net/amount/validate`, `Receipt::total/cargoOf/amount/validate/describe`,
`OpenReceipt::add/cancel/validateClose/close`, `Allocation::wholeWeighing/boxesOf`,
`Journal::balanceOf/loadOf/stockOf/unallocatedOf/destinationOf/waitingForAllocation/validateAllocation/validateCancel/validateShipment/shipmentOf`,
`Truck::depart/carryOver/remaining/isOverloaded/fillPercent`, `DayPrices::priceAt`,
`WorkDay::openReceipt/addWeighing/cancelWeighing/closeReceipt/validateClose/close/repeatTruck/adoptTruck/trucksForNextSession`,
`Uuid::generateV7/parse/toString/timestamp`, `StabilityDetector::push/isStable/stableWeight`,
`Supplier::matches`, `parseMoney/parseWeight`, `foldCase`.

### Інкапсуляція

- Усі поля класів `private`; назовні — лише `const`-геттери з `[[nodiscard]]`.
- Конструктор `Quantity` з числа приватний — значення лише через іменовані фабрики.
- `Transaction`: конструктор і копіювання `protected` — неможливий slicing; нащадки `final`.
- `Operator`: PIN не має геттера, лише `checkPin`.
- `Truck`: статус змінюється лише через `depart()` / `carryOver()` з перевіркою переходу.
- `Journal`: лише додавання; жодного методу видалення чи зміни запису.
- `WorkDay`: закрита сесія змінюється тільки перевантаженнями з `Amendment`.

## Лабораторна 2 — GUI 🔄

| Вимога | Мінімум | Стан |
|---|---|---|
| Логіка відділена від GUI | так | ядро без Qt; `Terminal` — тонкий міст, правила лише в `core/` ([ui.md](ui.md)) |
| Вікна / екрани | 4 | Вхід, Сесія, Прийом, Розподіл, пошук постачальника (+ діалоги закриття сесії і чека); Каса — заглушка |
| Елементи керування | 20 | > 20: `Button`, `TextField`, `ComboBox`, `RadioButton`, `TabButton`, `ToolButton`, `Dialog`… |
| Контейнер | 1 | `ListView` (постачальники, відкриті чеки, рядки чека, чеки до розподілу, фури/склад), `Repeater` (ціни, фури, культури із залишком) |
| Обробники подій | 10 | > 30 (`onClicked`, `onAccepted`, `Shortcut` F2, `Connections`…) |

Залишилось до альфи (23.10.2026): екрани Склад (відвантаження зі складу у фуру) і Каса.

## Лабораторна 3 — зовнішня бібліотека ⏳

Друк чека постачальнику без Qt (QPrinter/QPdfWriter не зараховуються):
**libharu** (PDF) + **nayuki qrcodegen** (QR з UUID чека); альтернатива — ESC/POS на термопринтер.
Звіт-ретроспектива — `docs/lab3_report.md`, список бібліотек — у README.
