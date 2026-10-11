# Розробка

## 1. Оточення

| Що | Версія / примітка |
|---|---|
| ОС | розробка на Fedora; будь-який Linux з Qt 6.8+ |
| Компілятор | з повною підтримкою C++20 і `<format>`: GCC 13+ (перевірено на GCC 16), Clang 17+ |
| CMake | ≥ 3.28 (у CLion — вбудований) |
| Генератор | Ninja |
| Qt | ≥ 6.8: Quick, QuickControls2 (далі знадобиться SerialPort); переклади `qt6-qttranslations` — необов'язково, для українських стандартних кнопок |
| SQLite | 3.51.2, амальгамація через `FetchContent` (системний `sqlite-devel` не потрібен); компілюється з `-w` — чужий код |
| GoogleTest | 1.17, завантажується через `FetchContent` — потрібна мережа при першій конфігурації |

## 2. Збірка

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/app/wms_terminal
```

Якщо `cmake` немає в `PATH`, але є CLion:

```bash
CM=~/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin
$CM/cmake -S . -B build -G Ninja && $CM/cmake --build build && $CM/ctest --test-dir build
```

| Ціль | Що це |
|---|---|
| `wms_core` | доменне ядро (статична бібліотека) |
| `wms_ui`, `wms_uiplugin` | QML-модуль `BerryWms.Ui` |
| `wms_style` | QML-модуль `BerryWms.Style`: власний стиль контролів, `Theme`, шрифт |
| `wms_terminal` | виконуваний файл термінала |
| `wms_infra` | інфраструктура: обгортка SQLite, схема, сховища журналу й сесій (статична бібліотека) |
| `sqlite3` | SQLite з амальгамації |
| `wms_core_tests`, `wms_infra_tests` | unit-тести ядра й інфраструктури |
| `wms_ui_qmllint` | перевірка QML-файлів `qmllint` |

Опції CMake: `WMS_BUILD_TESTS` (за замовчуванням `ON`).

Прапорці `-Wall -Wextra -Wpedantic -Wshadow` на всіх наших цілях — **збірка має бути без
попереджень**.

## 3. Тести

- GoogleTest, файли `tests/<модуль>_test.cpp`, виконувані `wms_core_tests` і `wms_infra_tests`
  (останній — на справжній SQLite: `:memory:` або тимчасовий файл);
  `gtest_discover_tests` реєструє кожен тест у CTest окремо.
- Покриваються ядро й infra (UI — тонкий шар без правил).
- Детермінованість: `Uuid::generateV7(time, randA, randB)` і явні `TimePoint` замість
  `Clock::now()` там, де час важливий.

| Файл | Що перевіряє |
|---|---|
| `quantity_test` | арифметика, округлення, форматування, `parseMoney` / `parseWeight` |
| `uuid_test` | структура v7, сортування за часом, `parse` / `toString` |
| `supplier_test` | пошук за id і частиною імені (кирилиця, регістр) |
| `operator_test` | формат PIN, `checkPin` |
| `transaction_test` | `Weighing`, `Receipt`, `Payment`: нетто, суми, валідація, анулювання, опт |
| `open_receipt_test` | додавання, анулювання, закриття з тим самим id |
| `scale_test` | симулятор, детектор стабільності |
| `truck_test` | ліміти, заповненість, переповнення, статуси |
| `allocation_test` | ціла палета, ящики, пропорційний поділ ваги |
| `shipment_test` | відвантаження зі складу |
| `journal_test` | баланс, завантаження, склад, ідемпотентність, усі `validate…` |
| `pricing_test` | історія цін, `priceAt` за часом |
| `workday_test` | сесія: ціни, фури, перенесення, відкриті чеки, закриття, поправки, `restore` |
| `sqlite_test` *(infra)* | обгортка: прив'язка й читання, `NULL`, помилки, зовнішні ключі, WAL, відкат і вкладені транзакції |
| `sqlite_journal_store_test` *(infra)* | кожен тип запису туди й назад, повторний UUID, перевідкриття файлу, відмова від новішої версії схеми |
| `sqlite_session_store_test` *(infra)* | знімок сесії (ціни, фури, відкриті чеки, поправки), перезапис, `current` / `lastClosed`, спільна база з журналом |

Запустити один набір:

```bash
./build/tests/wms_core_tests --gtest_filter='Journal*'
./build/tests/wms_infra_tests
```

Запуск термінала без вікна (перевірити, що QML завантажується без попереджень):

```bash
WMS_DATABASE=:memory: QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen timeout 3 build/app/wms_terminal
```

Без `QT_FORCE_STDERR_LOGGING` попередження Qt ідуть у journald.

## 4. Стиль коду

- Типи `PascalCase`, методи й змінні `camelCase`, поля з підкресленням у кінці (`amount_`),
  фабрики одиниць — функції (`kopecks()`, `grams()`).
- `[[nodiscard]]` на геттерах і обчисленнях; `noexcept`, де не кидає.
- Гроші — `Money` (копійки), вага — `Weight` (грами). **Жодних `float`/`double`** для грошей і ваги.
- Помилки програміста → винятки (`std::invalid_argument`, `std::logic_error`);
  помилки вводу оператора → `validate()` повертає список повідомлень українською.
- Коментарі англійською і лише про «чому», не про «що».
- `core/` не підключає Qt; UI не містить бізнес-правил.
- Код, ідентифікатори й коміти — англійською; інтерфейс і повідомлення оператору — українською.

## 5. Робочий процес

1. **Тест-першим для ядра:** спершу тест, що падає, потім мінімальна реалізація.
2. **Малі кроки:** кожен крок — окремий коміт зі змістовним повідомленням (вимога курсу).
3. Після змін у QML — `wms_ui_qmllint`.
4. Перед комітом: збірка без попереджень, усі тести зелені.
5. Нове рішення щодо предметної області фіксується в `CLAUDE.md` (розділ «Прийняті рішення»),
   а зміни API — у `docs/`.

## 6. Як додати нову можливість (приклад)

Припустімо, потрібен новий тип запису журналу, скажімо «повернення ягоди постачальнику»:

1. `tests/return_test.cpp` — тести на `validate()`, `balanceEffect()`, вплив на `stockOf`.
2. `core/include/wms/return.h` + `core/src/return.cpp` — клас `final : public Transaction`.
3. Нове значення в `TransactionKind`, перевантаження `Journal::add(Return)` і окремий вектор.
4. Оновити запити `Journal`, яких це стосується.
5. Додати файли в `core/CMakeLists.txt` і `tests/CMakeLists.txt`.
6. Лише потім — метод у `Terminal`, що повертає `QString` з помилкою, і кнопка в QML.
7. Оновити [domain.md](domain.md) і [course_mapping.md](course_mapping.md).
