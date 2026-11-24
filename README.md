# DukaanOS

A Point of Sale (POS) system currently being built with C++ and Qt.

## Current UI

- The app opens directly to a dashboard preview. New Order can scan a barcode or accept typed input and look up matching products in `Data/Product.txt`; order saving and billing are not connected.
- Use the role dropdown to switch between Cashier and Manager navigation.
- Dashboard stats are placeholders. Manager Products reads from `Data/Product.txt`; Order History reads saved order records. Held Orders and bill generation work as in-memory previews only, and no new order is written to storage or sent to a printer.
- Each page has its own file under `ui/pages/` and is assembled through `ui/ui.cpp`.
- The old BarcodeX screen is removed from the app window. Barcode code remains available for future POS work.

## Planned POS Pages

| Page | Access | Purpose |
|---|---|---|
| Login | Everyone | Sign in using a username and password from `Data/Users.txt`, then open the matching role dashboard |
| Dashboard | Cashier, Manager | Role-based actions and today's summary |
| Scan Product | Cashier, Manager | Scan a barcode and view product details |
| New Order | Cashier | Scan items, set payment, hold the order or generate the bill |
| Bill | Cashier | View the bill and download a print-ready A4 PDF |
| Held Orders | Cashier | Resume on-hold orders |
| Orders History | Cashier, Manager | Search and view orders; Manager can archive |
| Products | Manager | Search products, add/edit price, discount and stock, and view the low-stock list |

## Team

| Name | Responsibility |
|---|---|
| Shoaib Ul Hassan | UI, final integration, AuthService login and dashboard role routing |
| Umar Khan | Product features/data and user repository; product search, add/edit price, discount and stock, low-stock list, and Users.txt reading |
| Zain Ul Abideen | Order storage, order/bill IDs, POS barcode scanner adapter, barcode encoding/image generation and image-file handling |
| Anas | Payment processing, including card payments |
| Abdul Ahad | Bill generation/storage and order checkout flow (coordinating payment with Anas) |

Unassigned work (including shared record-codec changes, tests, documentation, bug fixes, and improvements) is split evenly across all five team members. Shoaib coordinates integration and shared interfaces; module owners remain responsible for their assigned behavior.

## Getting Started (Windows)

**Requirements:** Git and a Qt 6 MinGW 64-bit installation with the Widgets and Multimedia components. CMake, Ninja, and the matching MinGW compiler can be detected from the Qt installation.

1. Clone the repository.
2. Run `.\run.bat` from the project folder. It configures and builds the app, deploys the Qt runtime, and opens the UI.

If Qt is installed outside `C:\Qt`, set `QT_ROOT` to its folder or `QT_PREFIX_PATH` to the kit's `mingw_64` folder. If the matching compiler is not detected automatically, set `MINGW_ROOT` to its folder.

To build an installer, run `run.bat --package`, then compile `DukaanOS.iss` in Inno Setup to create `dist\DukaanOS-Setup.exe`.

## Project Structure

```
DukaanOS/
├── main.cpp
├── BarCode/
│   ├── common/      # Shared Image class
│   ├── encoder/     # Digits to Code 39 patterns
│   ├── generator/   # Patterns to barcode image
│   ├── scanner/     # Image to run widths
│   ├── camera/      # Live camera capture
│   └── decoder/     # Run widths to digits
├── POS/
│   ├── bill/        # Bill records, storage, and calculations
│   ├── common/      # Record codec and ID generation
│   ├── order/       # Order records, storage, and checkout services
│   ├── product/     # Product records and stock services
│   ├── scanner/     # POS adapter for the camera barcode scanner
│   └── user/        # User records and login authentication
├── ui/
│   ├── assets/      # App icons and page images
│   ├── pages/       # One source file per UI page
│   ├── ui.cpp       # Integrates pages into the main window
│   └── ui.h
├── Data/            # Product.txt, Order.txt, OrderDetail.txt, Bill.txt, Users.txt
├── barcodes/        # Product barcode images
└── tests/
```

Pages handle display and input, services hold business rules, and repositories read and write files. Work can span folders when needed to deliver an assigned feature. Coordinate shared-header and interface changes with Shoaib before integration.

## Data Files

One record per line, fields separated by a space, each line ending with `#`. Names use `_` instead of spaces, and an empty optional field is written as `-`.

| File | Fields |
|---|---|
| PCategory.txt | category id (2 digits), category name, category acronym |
| Product.txt | id, barcode (category id + 4-digit sequence), acronym (max 8), name, unit, unit price, tax (PKR), discount per unit, available quantity, barcode image path, category id |
| Order.txt | order id (8 digits), date, time, customer name, contact, status, payment amount, payment mode, merchant |
| OrderDetail.txt | order id, product id, barcode, quantity |
| Bill.txt | bill id (9 digits), customer name, order id, acronym, unit price, discount per unit, quantity, total without tax, total tax, total discount, total payable, status |
| Users.txt | username, password, role |

Order status: `in-progress`, `on-hold`, `paid`, `archived`. Payment mode: `cash`, `card`, `wallet`.

### Order reports and editing (POS service API)

Shoaib's UI can call `readIncompleteOrdersReport(report, error)` and `readCompletedOrdersReport(report, error)`. Each `OrderReport` has the matching `orders` and their summed `totalAmount` (from `Order.txt`).

For editing, call `openOrderForEditing(orderId, false, session, error)`. On-hold orders open without approval. For a paid order, the service returns an approval message if it was paid within 7 days; after the UI authenticates a manager, retry with `managerApproved=true`. The UI then edits `session.items` and calls `saveOrderEdits(session, managerApproved, error)`. Paid edits adjust stock by the quantity difference; on-hold edits do not change stock. Orders paid more than 7 days ago are refused even with approval.

`Users.txt` credentials are plain-text demonstration data for local UI testing only. Do not use real or reused passwords.

## Workflow

Work on your own `feature/<name>` branch, merge the latest `main` into it, push, and open a pull request. Do not commit directly to `main`.

```
git switch main && git pull origin main
git switch feature/<name>
git merge main
git add <files> && git commit -m "message"
git push -u origin feature/<name>
```

## Pull Request Checks

Every pull request to `main` automatically runs a basic Windows CI check:

- Qt and CMake configure
- DukaanOS application build
- Existing POS tests
- `ZainRequirementsTest` for category-prefixed barcode images, order reports, and edit rules
- Barcode encoder, generator, scanner, and decoder round-trip test

Run the full local suite with `ctest --test-dir build/<configured-build-directory> --output-on-failure`.
A pull request should only be merged when the CI check passes. Detailed manual tests such as camera hardware, PDF appearance, reports, and end-to-end sales are completed separately before release.

## License

Released under the terms of the `LICENSE` file.
