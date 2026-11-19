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

-- Dashboard rollups (run 4). Per-day aggregates of `events`, kept exact by the trigger below, so the dashboard
-- reads ~hundreds of rows instead of scanning every event in the range. WITHOUT ROWID + day-first keys make
-- each range query a covering, clustered range scan. `events` is append-only (no app ever updates/deletes it).
CREATE TABLE ev_day (
  day INTEGER PRIMARY KEY, events INTEGER NOT NULL, revenue_cents INTEGER NOT NULL
) WITHOUT ROWID;
CREATE TABLE ev_country_day (
  day INTEGER NOT NULL, country TEXT NOT NULL, events INTEGER NOT NULL, PRIMARY KEY (day, country)
) WITHOUT ROWID;
CREATE TABLE ev_item_day (
  day INTEGER NOT NULL, item TEXT NOT NULL, revenue_cents INTEGER NOT NULL, purchases INTEGER NOT NULL,
  PRIMARY KEY (day, item)
) WITHOUT ROWID;
-- Per user: last day with any event / with a purchase. Exact distinct-user counts for a "last N days" window:
-- the window always ends at the anchor ("now") and no event is at or after it, so a user was active in the
-- window iff last_day >= window start (an indexed range count instead of a DISTINCT over every user-day).
CREATE TABLE ev_user (
  user_id INTEGER PRIMARY KEY, last_day INTEGER NOT NULL, last_purchase_day INTEGER
);
CREATE INDEX ev_user_last ON ev_user(last_day);
CREATE INDEX ev_user_last_purchase ON ev_user(last_purchase_day);

CREATE TRIGGER events_rollup AFTER INSERT ON events BEGIN
  INSERT INTO ev_day VALUES (NEW.ts / 86400, 1, NEW.revenue_cents)
    ON CONFLICT (day) DO UPDATE SET events = events + 1, revenue_cents = revenue_cents + excluded.revenue_cents;
  INSERT INTO ev_country_day VALUES (NEW.ts / 86400, NEW.country, 1)
    ON CONFLICT (day, country) DO UPDATE SET events = events + 1;
  INSERT INTO ev_item_day SELECT NEW.ts / 86400, NEW.item, NEW.revenue_cents, 1 WHERE NEW.type = 'purchase'
    ON CONFLICT (day, item) DO UPDATE SET revenue_cents = revenue_cents + excluded.revenue_cents, purchases = purchases + 1;
  INSERT INTO ev_user VALUES (NEW.user_id, NEW.ts / 86400, CASE WHEN NEW.type = 'purchase' THEN NEW.ts / 86400 END)
    ON CONFLICT (user_id) DO UPDATE SET last_day = max(last_day, excluded.last_day),
      last_purchase_day = NULLIF(max(COALESCE(last_purchase_day, -1), COALESCE(excluded.last_purchase_day, -1)), -1);
END;
