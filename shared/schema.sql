PRAGMA journal_mode = WAL;

CREATE TABLE categories (
  id   INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  slug TEXT NOT NULL UNIQUE
);

CREATE TABLE products (
  id          INTEGER PRIMARY KEY,
  category_id INTEGER NOT NULL REFERENCES categories(id),
  name        TEXT NOT NULL,
  description TEXT NOT NULL,
  price_cents INTEGER NOT NULL,
  stock       INTEGER NOT NULL,
  rating      REAL NOT NULL,
  image       TEXT NOT NULL
);
CREATE INDEX products_category ON products(category_id);
CREATE INDEX products_price ON products(price_cents);

CREATE TABLE cart_items (
  id         INTEGER PRIMARY KEY,
  session_id TEXT NOT NULL,
  product_id INTEGER NOT NULL REFERENCES products(id),
  qty        INTEGER NOT NULL CHECK (qty > 0),
  UNIQUE (session_id, product_id)
);

CREATE TABLE boards (
  id   INTEGER PRIMARY KEY,
  name TEXT NOT NULL
);

CREATE TABLE cards (
  id          INTEGER PRIMARY KEY,
  board_id    INTEGER NOT NULL REFERENCES boards(id),
  col         TEXT NOT NULL CHECK (col IN ('backlog', 'todo', 'doing', 'done')),
  position    INTEGER NOT NULL,
  title       TEXT NOT NULL,
  description TEXT NOT NULL,
  labels      TEXT NOT NULL,
  assignee    TEXT NOT NULL,
  due_date    TEXT NOT NULL
);
CREATE INDEX cards_board ON cards(board_id, col, position);

CREATE TABLE events (
  id            INTEGER PRIMARY KEY,
  ts            INTEGER NOT NULL,
  user_id       INTEGER NOT NULL,
  type          TEXT NOT NULL CHECK (type IN ('view', 'signup', 'purchase')),
  country       TEXT NOT NULL,
  item          TEXT NOT NULL,
  revenue_cents INTEGER NOT NULL
);
CREATE INDEX events_ts ON events(ts);
