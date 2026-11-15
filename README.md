# DukaanOS

A barcode based Point of Sale (POS) system built with C++ and Qt. Products are identified by camera scanning Code 39 barcodes, using an encoder and decoder written from scratch.

## Features

- Login with Cashier and Manager roles
- Camera based barcode scanning to view product details
- Product management: searchable product table, add/edit price, discount and stock, plus a low-stock list
- Order recording with stock check, hold and payment (cash, card, wallet)
- Bill generation with order, tax and discount totals
- Text file storage for products, orders, order details and bills
- Code 39 barcode generator and scanner (six-digit numeric barcodes)

## Pages

| Page | Access | Purpose |
|---|---|---|
| Login | Everyone | Sign in with username and password |
| Dashboard | Cashier, Manager | Role based actions and today's summary |
| Scan Product | Cashier, Manager | Scan a barcode and view product details |
| New Order | Cashier | Scan items, set payment, hold the order or generate the bill |
| Bill | Cashier | View the generated bill |
| Held Orders | Cashier | Resume on-hold orders |
| Orders History | Cashier, Manager | Search and view orders; Manager can archive |
| Products | Manager | Search products, add/edit price, discount and stock, and view the low-stock list |

## Team

| Name | Responsibility |
|---|---|
| Shoaib Ul Hassan (Team Leader) | UI, final integration, and the login feature |
| Umar Khan | Product and user features/data, including product search, add/edit price, discount and stock, and the low-stock list; product UI is coordinated with Shoaib |
| Zain Ul Abideen | Order storage, order/bill IDs, barcode image generation and image-file handling |
| Anas | Payment processing, including card payments |
| Abdul Ahad | Bill generation and checkout flow |

Unassigned work (including shared record-codec changes, tests, documentation, bug fixes, and improvements) is split evenly across all five team members. Shoaib coordinates integration and shared interfaces; module owners remain responsible for their assigned behavior.

## Getting Started (Windows)

**Requirements:** Git, CMake, Ninja, and Qt 6 (Widgets and Multimedia) with the MinGW 64-bit kit.

1. Clone the repository.
2. Run `run.bat`. It configures, builds, deploys the Qt runtime and starts the app.

If Qt is not in `C:\Qt`, set `QT_ROOT` (Qt folder) or `QT_PREFIX_PATH` (the kit's `mingw_64` folder). If several MinGW compilers are installed, set `MINGW_ROOT` to the matching one.

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
├── ui/              # Barcode and POS windows
├── Data/            # Product.txt, Order.txt, OrderDetail.txt, Bill.txt, Users.txt
├── barcodes/        # Product barcode images
└── tests/
```

Pages handle display and input, services hold business rules, and repositories read and write files. Work can span folders when needed to deliver an assigned feature. Coordinate shared-header and interface changes with Shoaib before integration.

## Data Files

One record per line, fields separated by a space, each line ending with `#`. Names use `_` instead of spaces, and an empty optional field is written as `-`.

| File | Fields |
|---|---|
| Product.txt | id, barcode, acronym (max 8), name, unit, unit price, tax (PKR), discount per unit, available quantity, barcode image path |
| Order.txt | order id (8 digits), date, time, customer name, contact, status, payment amount, payment mode, merchant |
| OrderDetail.txt | order id, product id, barcode, quantity |
| Bill.txt | bill id (9 digits), customer name, order id, acronym, unit price, discount per unit, quantity, total without tax, total tax, total discount, total payable, status |
| Users.txt | username, password, role |

Order status: `in-progress`, `on-hold`, `paid`, `archived`. Payment mode: `cash`, `card`, `wallet`.

## Workflow

Work on your own `feature/<name>` branch, merge the latest `main` into it, push, and open a pull request. Do not commit directly to `main`.

```
git switch main && git pull origin main
git switch feature/<name>
git merge main
git add <files> && git commit -m "message"
git push -u origin feature/<name>
```

## License

Released under the terms of the `LICENSE` file.
