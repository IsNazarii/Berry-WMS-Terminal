# Розробка

## 1. Оточення

| Що | Версія / примітка |
|---|---|
| ОС | перевірено на Fedora (Qt 6.11, GCC 16) і Ubuntu 26.04 (Qt 6.10, GCC 15); будь-який Linux з Qt 6.8+ |
| Компілятор | з повною підтримкою C++20 і `<format>`: GCC 13+ (перевірено на GCC 16), Clang 17+ |
| CMake | ≥ 3.28 (у CLion — вбудований) |
| Генератор | Ninja |
| Qt | ≥ 6.8: Quick, QuickControls2, плагін SVG (`qt6-qtsvg` — іконки панелі), SerialPort (драйвер ваг, ціль `wms_serial`), PrintSupport (друк чека, ціль `wms_print`); переклади `qt6-qttranslations` — необов'язково, для українських стандартних кнопок |
| SQLite | 3.51.2, амальгамація через `FetchContent` (системний `sqlite-devel` не потрібен); компілюється з `-w` — чужий код |
| GoogleTest | 1.17, завантажується через `FetchContent` — потрібна мережа при першій конфігурації |
| qrcodegen | nayuki QR-Code-generator 1.8.0 через `FetchContent` (архів з `URL_HASH`); власного CMake у бібліотеки нема — ціль `qrcodegen` з одного `.cpp`, з `-w` |
| Порт ваг | оригінальний кабель «Промприлад» USB↔DB9 (FTDI) або USB↔RS-232 + 3-провідний null-modem DB9F–DB9F. **Права на порт надати явно**: `sudo usermod -aG dialout $USER` і перелогінитись — інакше порт не відкриється, а на табло буде «—». Деталі — [hardware/scale-ie04.md](hardware/scale-ie04.md#права-на-порт--надати-явно) |

Ubuntu 26.04, усе потрібне одним рядком (`faketime` і `python3-pil` — лише для знімків екранів, §5):

```bash
sudo apt install git cmake ninja-build build-essential qt6-base-dev qt6-declarative-dev qt6-serialport-dev \
  qt6-translations-l10n qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
  qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtqml qml6-module-qtqml-workerscript \
  qml6-module-qtcore faketime python3-pil
```

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
| `wms_infra` | інфраструктура: обгортка SQLite, схема, сховища журналу, сесій і довідників, бекап (статична бібліотека) |
| `wms_serial` | драйвер ваг ІЕ-04 на `QSerialPort` — окремо, щоб `wms_infra` і його тести були без Qt |
| `wms_print` | друк чека: `QrMatrix` (qrcodegen), `paintReceipt`, `ReceiptPrinter` (Qt PrintSupport) |
| `sqlite3`, `qrcodegen` | SQLite з амальгамації; генератор QR |
| `wms_core_tests`, `wms_infra_tests`, `wms_serial_tests`, `wms_print_tests` | unit-тести ядра, інфраструктури, драйвера ваг і друку |
| `wms_ui_qmllint` | перевірка QML-файлів `qmllint` |
| `wms_shots` | знімки всіх екранів без вікна для перевірки змін UI «до/після» (`tools/shots/`; збирається, якщо є Qt Test) |

Опції CMake: `WMS_BUILD_TESTS`, `WMS_BUILD_TOOLS` (`wms_shots`), `WMS_INSTALL_UDEV_RULE` (усі за замовчуванням `ON`).

### Встановлення й пакети

Пакет системи, на якій запущено скрипт (`.deb` на Ubuntu, `.rpm` на Fedora), — однією командою:

```bash
packaging/build-packages.sh          # Release + усі тести → build-release/berry-wms_<версія>_amd64.deb або .rpm
sudo apt install ./build-release/berry-wms_0.1.0_amd64.deb      # Ubuntu
sudo dnf install ./build-release/berry-wms-0.1.0-1.x86_64.rpm   # Fedora
```

Потрібні `dpkg-dev` (Ubuntu) або `rpm-build` (Fedora). Пакет ставить:

| Файл | Навіщо |
|---|---|
| `/usr/bin/berry-wms` | програма (у збірці — `wms_terminal`) |
| `/usr/share/applications/io.github.isnazarii.BerryWms.desktop` | ярлик у меню; `main.cpp` ставить ту саму назву (`setDesktopFileName`), щоб вікно мало іконку |
| `/usr/share/icons/hicolor/scalable/apps/io.github.isnazarii.BerryWms.svg` | іконка — знак `BrandMark` у кольорах екрана входу |
| `/usr/share/doc/berry-wms/` | `LICENSE.md` і `THIRD_PARTY_NOTICES.md`: ліцензії сторонніх частин мають іти з кожною копією |
| `/usr/lib/udev/rules.d/70-berry-wms-scale.rules` | кабель ваг (FTDI `0403:6001`) належить тому, хто увійшов у систему: без групи `dialout` і перезавантаження; діє після перепідключення кабелю |

Qt береться з системи, тому пакет — для того самого випуску дистрибутива (чи новішого), де зібраний: потрібен
Qt ≥ 6.8 (Fedora 42+, Ubuntu 26.04+). Пакет вимагає Qt «цієї версії або новішої», тож зібраний на найстарішому
підтримуваному випуску ставиться й на всі новіші, а оновлення Qt у системі його не ламає. Для цього QML
компілюється лише в байткод (`QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode"` у `ui/CMakeLists.txt` і
`ui/style/CMakeLists.txt`): компіляція QML у C++ (AOT) кличе приватний API Qt і прив'язала б пакет до точної
версії (`qt6-declarative-private-abi (= 6.10.2)`). Якщо Qt у системі новіший за байткод, Qt компілює QML з
тексту, вбудованого в програму, під час запуску. Перевірка: `objdump -T build-release/app/wms_terminal | grep -c PRIVATE_API` → `0`.
QML-модулі й плагін SVG програма завантажує під час роботи — їх перелічено в залежностях пакета вручну
(`packaging/CMakeLists.txt`); новий `import` у QML — додати й туди. База (`~/.local/share/BerryWms/`) лежить
окремо від програми: встановлення, оновлення й видалення пакета її не чіпають.

**AppImage** — один файл з Qt усередині, для будь-якого свіжого Linux (без встановлення й без `sudo`):

```bash
packaging/build-appimage.sh          # Release + усі тести → build-appimage/Berry_WMS_Terminal-<версія>-x86_64.AppImage
chmod +x Berry_WMS_Terminal-*.AppImage && ./Berry_WMS_Terminal-*.AppImage
```

Скрипт сам завантажує закріплені версії `linuxdeploy`, `linuxdeploy-plugin-qt` і `appimagetool` (перевірка
SHA-256, кеш — `~/.cache/berry-wms/appimage-tools`). `appimagetool` 1.9 кладе новий статичний runtime, тож
машині користувача `libfuse2` не потрібна. Плагін Qt не бере клієнтських плагінів Wayland (без
`wayland-shell-integration` вікно на Wayland не відкривається), тому скрипт додає їх сам. Разом з Qt (LGPL) в
образ ідуть тексти LGPL-3 і GPL-3. Правила udev в AppImage нема — для ваг потрібна група `dialout`.

AppImage працює лише там, де glibc не старіша, ніж на машині збирання: зібраний на Ubuntu 26.04 (glibc 2.43)
не запуститься на 24.04. Тому образ для релізу збирає CI на найстарішій підтримуваній системі (Ubuntu 24.04,
Qt з qt.io); локальна збірка — для перевірки. Запуск без вікна (`QT_QPA_PLATFORM=offscreen`) AppImage не
вміє — у ньому лише плагіни Wayland і X11.

**Релізи (GitHub Actions, `.github/workflows/release.yml`).** Версія — у `project(BerryWms VERSION …)` у
`CMakeLists.txt`. Щоб випустити її:

```bash
git tag v0.2.0 && git push origin v0.2.0     # тег має збігатися з версією в CMakeLists.txt, інакше збірка зупиниться
```

CI тими самими скриптами (`packaging/`) і лише після зелених тестів збирає `.deb` для Ubuntu 26.04 (у контейнері
`ubuntu:26.04`; ставиться й на новіші), `.rpm` для Fedora 44 і новіших (контейнер `fedora:44`) і AppImage
(Ubuntu 24.04, Qt 6.11.3 з qt.io, GCC 14) і викладає їх на сторінку Releases з таблицею «який файл узяти».
Ручний запуск (Actions → Release → Run workflow) лише збирає: файли лишаються артефактами запуску, релізу нема.
Коли найстаріший підтримуваний випуск Fedora чи Ubuntu знято з підтримки — підняти контейнер до наступного.

Без пакета, у домашню теку (правило udev там нічого б не дало, тому його вимкнено):

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DWMS_INSTALL_UDEV_RULE=OFF
cmake --build build-release && cmake --install build-release --prefix ~/.local
```

Прапорці `-Wall -Wextra -Wpedantic -Wshadow` на всіх наших цілях — **збірка має бути без
попереджень**.

## 3. Тести

- GoogleTest, файли `tests/<модуль>_test.cpp`, виконувані `wms_core_tests`, `wms_infra_tests`
  (на справжній SQLite: `:memory:` або тимчасовий файл), `wms_serial_tests` (псевдотермінал) і
  `wms_print_tests` (власний `main()` з `QGuiApplication` — шрифтам він потрібен);
  `gtest_discover_tests` реєструє кожен тест у CTest окремо.
- Покриваються ядро й infra (UI — тонкий шар без правил). Зараз 356 тестів: 293 ядра, 42 infra, 7 порту ваг, 14 друку.
- Детермінованість: `Uuid::generateV7(time, randA, randB)` і явні `TimePoint` замість
  `Clock::now()` там, де час важливий.

| Файл | Що перевіряє |
|---|---|
| `quantity_test` | арифметика, округлення, форматування (і екранне `displayKilograms` / `displayHryvnias`), `parseMoney` / `parseWeight` |
| `uuid_test` | структура v7, сортування за часом, `parse` / `toString` |
| `supplier_test` | пошук за id і частиною імені (кирилиця, регістр), `validateNewSupplier`, `nextSupplierId` |
| `crop_test` | `validateNewCrop` (дублі після нормалізації, назва прихованої вільна), `cropCodeFor`, `validateRemoveCrop` |
| `crop_keys_test` | кнопки культур: заміна найдавнішої, `fill` з довідника |
| `operator_test` | формат PIN, `checkPin` |
| `transaction_test` | `Weighing`, `Receipt`, `Payment` (`paidBy`): нетто, суми, валідація, анулювання, опт |
| `open_receipt_test` | додавання, анулювання, `crops` / `setPrice`, закриття з тим самим id і сесією |
| `session_summary_test` | підсумок дня: лише записи своєї сесії + відкриті чеки, культури, де ящики, виплати за способом |
| `loading_history_test` | історія вантаження: постачальник і № палети, ящиками, сторно зі слідом, склад лише свого дня (і до закриття), фура за весь рейс, хто вантажив |
| `scale_test` | симулятор (стабільність після 3 однакових показів, скидання при закритті), детектор стабільності |
| `ie04_test` | кадри ваг ІЕ-04: вага й лампи, кома з бітів статусу, мінус, нечислове табло, потік зі сміттям, увесь запис з пристрою |
| `truck_test` | ліміт у ящиках, заповненість, переповнення, статуси |
| `allocation_test` | ціла палета, ящики, пропорційний поділ ваги, хто вантажив / зняв сторно |
| `shipment_test` | відвантаження зі складу, хто відвантажив |
| `journal_test` | баланс, завантаження, склад, ідемпотентність, сторно, `mentionsCrop`, `paymentFor`, усі `validate…` |
| `receipt_document_test` | що на папері: назви культур, без анульованих і без дірок у нумерації, підсумки, опт без цін, виплата, копія |
| `pricing_test` | історія цін, `priceAt` за часом |
| `workday_test` | сесія: ціни, фури, перенесення, відкриті чеки, авторозподіл, «Оплатити», анулювання зі сторно, закриття, поправки, `restore` |
| `sqlite_test` *(infra)* | обгортка: прив'язка й читання, `NULL`, помилки, зовнішні ключі, WAL, відкат і вкладені транзакції |
| `sqlite_journal_store_test` *(infra)* | кожен тип запису туди й назад (виплата — зі своїм чеком; розподіл, сторно, відвантаження — з оператором або без), повторний UUID, перевідкриття файлу, відмова від новішої версії схеми |
| `sqlite_session_store_test` *(infra)* | знімок сесії (ціни, фури, відкриті чеки, поправки), перезапис, `current` / `lastClosed` / `closedSessions` / `find`, спільна база з журналом |
| `sqlite_repository_test` *(infra)* | довідники `Supplier` / `Operator` / `Crop`: upsert, порядок, `remove`, `archived` |
| `backup_test` *(infra)* | копія через `VACUUM INTO` відкривається, ротація лишає `keep` останніх і не чіпає чужі файли |
| `receipt_printer_test` *(print)* | QR (розмір символу, шукачі, різний текст — різний символ); чек на зображенні 1–90 палет у межах полів (A4 і A5), без порожніх аркушів, QR у правому верхньому куті; PDF у теку |
| `serial_scale_driver_test` *(serial)* | `SerialScaleDriver` на псевдотерміналі, що грає ваги: останній кадр, тиша й висмикнутий кабель закривають порт, повторне відкриття, увесь запис |

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

Друк без паперу — чеки стають PDF-файлами в теці; так само тести друку (`wms_print_tests`) ніколи не чіпають принтер:

```bash
WMS_PRINT_PDF=/tmp/receipts build/app/wms_terminal
WMS_RECEIPT_SAMPLES=/tmp/receipts build/tests/wms_print_tests   # лишає зразки чеків
```

Зі справжніми вагами — порт через змінну (налаштування `scale/port` не чіпається):

```bash
WMS_SCALE_PORT=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_AN6UY90E-if00-port0 ./build/app/wms_terminal
```

Вивід термінала, якого вбиває `timeout`, писати у файл, а не в конвеєр (`| grep`) — інакше буфер губиться.

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
2. **Малі кроки:** кожен крок — окремий коміт зі змістовним повідомленням.
3. Після змін у QML — `wms_ui_qmllint`; зміни, що не мають міняти вигляд (рефакторинг), — знімки до й після:
   ```bash
   tools/shots/shots.sh /tmp/before            # до змін (після збірки)
   tools/shots/shots.sh /tmp/after             # після змін і перезбірки
   python3 tools/shots/compare.py /tmp/before /tmp/after
   ```
   Знімки: вхід, шість вкладок через F5–F10, діалоги меню ⋯ і «Чек не надруковано» — світла й темна тема,
   ширина 1024 і 1536; база в пам'яті з демо-даними, масштаб 100%, налаштування оператора не зачіпаються.
   Годинник процесу щоразу стартує з 13.11.2026 10:00 (libfaketime — пакет `faketime` на Ubuntu, `libfaketime` на Fedora),
   тож без змін UI знімки збігаються попіксельно; без faketime скрипт попереджає, і різниця в часі чека — норма.
   Для `compare.py` потрібен Pillow (`python3-pil`).
   Знімки для README — `tools/shots/readme.sh`: населений робочий день (14.07.2026, кілька постачальників, фури, склад,
   оплати, «підвішені» палети), 1280×800 з подвійною щільністю, обидві теми; `frame.py` додає кути, рамку й тінь і
   кладе файли в `docs/screenshots/<екран>-light|dark.png`. Ваги дає симулятор, тож числа щоразу трохи інші.
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
7. Оновити [domain.md](domain.md) (а якщо з'явився новий клас чи ієрархія — і [course/mapping.md](course/mapping.md)).
