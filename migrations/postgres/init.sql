-- EFX PostgreSQL Schema
-- Configuration & state data

CREATE TABLE tiers (
    tier_id       VARCHAR(32) PRIMARY KEY,
    label         VARCHAR(64) NOT NULL,
    multiplier    DOUBLE PRECISION NOT NULL,
    sort_order    INTEGER NOT NULL
);

INSERT INTO tiers (tier_id, label, multiplier, sort_order) VALUES
    ('platinum',   'Platinum',   1.0, 1),
    ('gold',       'Gold',       1.5, 2),
    ('silver',     'Silver',     2.5, 3),
    ('bronze',     'Bronze',     4.0, 4),
    ('restricted', 'Restricted', 8.0, 5);

CREATE TABLE clients (
    client_id     VARCHAR(64) PRIMARY KEY,
    name          VARCHAR(256) NOT NULL,
    client_type   VARCHAR(32) NOT NULL DEFAULT 'standard',
    default_tier  VARCHAR(32) NOT NULL DEFAULT 'silver' REFERENCES tiers(tier_id),
    alpha_score   DOUBLE PRECISION DEFAULT 0.0,
    toxicity      VARCHAR(16) DEFAULT 'low',
    is_important  BOOLEAN DEFAULT false,
    is_active     BOOLEAN DEFAULT true,
    notes         TEXT,
    created_at    TIMESTAMPTZ DEFAULT NOW(),
    updated_at    TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE client_pair_overrides (
    client_id      VARCHAR(64) REFERENCES clients(client_id) ON DELETE CASCADE,
    pair           VARCHAR(8) NOT NULL,
    tier_override  VARCHAR(32) REFERENCES tiers(tier_id),
    adjustment_pct DOUBLE PRECISION DEFAULT 0.0,
    is_active      BOOLEAN DEFAULT true,
    updated_at     TIMESTAMPTZ DEFAULT NOW(),
    PRIMARY KEY (client_id, pair)
);

CREATE TABLE sessions (
    session_id    VARCHAR(32) PRIMARY KEY,
    label         VARCHAR(64) NOT NULL,
    hub_city      VARCHAR(32) NOT NULL,
    start_utc     TIME NOT NULL,
    end_utc       TIME NOT NULL,
    spread_multiplier DOUBLE PRECISION DEFAULT 1.0,
    is_active     BOOLEAN DEFAULT true,
    config_json   JSONB DEFAULT '{}'::jsonb
);

INSERT INTO sessions (session_id, label, hub_city, start_utc, end_utc, spread_multiplier) VALUES
    ('new_york',  'New York',  'New York',  '14:00', '21:00', 1.2),
    ('singapore', 'Singapore', 'Singapore', '21:00', '06:00', 1.5),
    ('london',    'London',    'London',    '06:00', '14:00', 1.0);

CREATE TABLE session_pair_config (
    session_id    VARCHAR(32) REFERENCES sessions(session_id),
    pair          VARCHAR(8) NOT NULL,
    is_active     BOOLEAN DEFAULT true,
    spread_override DOUBLE PRECISION,
    PRIMARY KEY (session_id, pair)
);

CREATE TABLE session_client_config (
    session_id    VARCHAR(32) REFERENCES sessions(session_id),
    client_id     VARCHAR(64) REFERENCES clients(client_id),
    is_active     BOOLEAN DEFAULT true,
    PRIMARY KEY (session_id, client_id)
);

CREATE TABLE tier_change_log (
    id            SERIAL PRIMARY KEY,
    client_id     VARCHAR(64) REFERENCES clients(client_id),
    pair          VARCHAR(8),
    old_tier      VARCHAR(32),
    new_tier      VARCHAR(32),
    reason        TEXT,
    changed_by    VARCHAR(64) DEFAULT 'system',
    created_at    TIMESTAMPTZ DEFAULT NOW()
);

-- Seed some example clients
INSERT INTO clients (client_id, name, client_type, default_tier, alpha_score, toxicity, is_important) VALUES
    ('hedge_fund_alpha',  'Alpha Capital Partners',  'hedge_fund',    'gold',       2.1,  'medium', false),
    ('real_money_pension', 'Global Pension Fund',     'real_money',    'platinum',   -0.8, 'low',    true),
    ('corp_treasury_acme', 'Acme Corp Treasury',      'corporate',     'silver',     -0.3, 'low',    false),
    ('toxic_hft_firm',     'Velocity Trading LLC',    'hft',           'restricted', 5.2,  'high',   false),
    ('retail_aggregator',  'FX Connect Retail',       'retail_agg',    'silver',     0.1,  'low',    false),
    ('sovereign_fund',     'Norges Investment Mgmt',  'sovereign',     'platinum',   -0.5, 'low',    true),
    ('algo_firm_momentum', 'Quant Momentum Capital',  'algo',          'bronze',     3.8,  'high',   false),
    ('bank_flow_dealer',   'Regional Bank Treasury',  'bank',          'gold',       0.3,  'low',    true);
