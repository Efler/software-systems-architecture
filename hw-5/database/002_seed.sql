INSERT INTO users (login, first_name, last_name, email)
VALUES
    ('ivanov', 'Иван', 'Иванов', 'ivanov@example.com'),
    ('petrova', 'Анна', 'Петрова', 'petrova@example.com')
ON CONFLICT (login) DO NOTHING;

INSERT INTO services (name, description, price, duration_minutes)
VALUES
    ('Ремонт розетки', 'Замена или ремонт бытовой электрической розетки', 1800.00, 60),
    ('Уборка квартиры', 'Поддерживающая уборка квартиры до 60 кв. м', 3500.00, 180),
    ('Настройка Wi-Fi', 'Настройка роутера и базовая диагностика сети', 2200.00, 90),
    ('Сборка шкафа', 'Сборка корпусной мебели по инструкции', 3000.00, 120)
ON CONFLICT DO NOTHING;
