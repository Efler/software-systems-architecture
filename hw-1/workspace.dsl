workspace "Система управления проектами" "Архитектура системы управления проектами и задачами" {

    model {
        projectMember = person "Участник проекта" "Работает с проектами и задачами, просматривает задачи проекта и получает задачи по коду"
        projectManager = person "Руководитель проекта" "Создает проекты и задачи, назначает исполнителей и контролирует работу команды"
        admin = person "Администратор системы" "Создает пользователей и управляет базовыми настройками доступа"

        authSystem = softwareSystem "Сервис корпоративной аутентификации" "Проверяет учетные данные пользователей и токены доступа"
        emailSystem = softwareSystem "Email-сервис" "Отправляет пользователям уведомления о проектах, задачах и назначениях"
        messengerSystem = softwareSystem "Корпоративный мессенджер" "Доставляет быстрые уведомления командам и исполнителям"
        vcsSystem = softwareSystem "Система контроля версий" "Хранит ветки, коммиты и pull request, связанные с задачами"

        projectManagementSystem = softwareSystem "Система управления проектами" "Позволяет создавать пользователей, проекты и задачи, искать пользователей и проекты, получать задачи проекта и отдельные задачи по коду" {
            webApp = container "Web Application" "Веб-интерфейс для работы с пользователями, проектами и задачами" "React, TypeScript" "WebBrowser"
            apiGateway = container "API Gateway" "Единая точка входа для API-запросов клиентского приложения" "Kong, Nginx, REST API" "API"

            userService = container "User Service" "Создает пользователей, ищет пользователей по логину, имени и фамилии" "Python, FastAPI, REST API" "Service"
            projectService = container "Project Service" "Создает проекты, ищет проекты по имени и возвращает список проектов" "Python, FastAPI, REST API" "Service"
            taskService = container "Task Service" "Создает задачи в проекте, возвращает задачи проекта и задачу по коду" "Python, FastAPI, REST API" "Service"
            notificationService = container "Notification Service" "Обрабатывает события системы и отправляет уведомления" "Python, FastAPI, AMQP Consumer" "Service"

            userDB = container "User Database" "Хранит данные пользователей" "PostgreSQL" "Database"
            projectDB = container "Project Database" "Хранит данные проектов" "PostgreSQL" "Database"
            taskDB = container "Task Database" "Хранит данные задач" "PostgreSQL" "Database"

            cache = container "Cache" "Кэширует пользователей по логину, проекты по имени и список проектов" "Redis" "Cache"
            messageQueue = container "Message Queue" "Передает доменные события между сервисами" "RabbitMQ" "Queue"
        }

        projectMember -> projectManagementSystem "Работает с проектами и задачами через API"
        projectManager -> projectManagementSystem "Создает проекты, задачи и назначает исполнителей"
        admin -> projectManagementSystem "Создает пользователей и управляет доступом"

        projectManagementSystem -> authSystem "Проверяет учетные данные и токены доступа" "OAuth2/OIDC"
        projectManagementSystem -> emailSystem "Отправляет email-уведомления" "SMTP"
        projectManagementSystem -> messengerSystem "Отправляет уведомления в командные каналы" "HTTPS/REST"
        projectManagementSystem -> vcsSystem "Получает сведения о ветках, коммитах и pull request" "HTTPS/REST"

        projectMember -> webApp "Работает с проектами и задачами" "HTTPS"
        projectManager -> webApp "Создает проекты и задачи" "HTTPS"
        admin -> webApp "Создает пользователей" "HTTPS"

        webApp -> apiGateway "Отправляет API-запросы" "HTTPS/REST"

        apiGateway -> authSystem "Проверяет токены доступа" "OAuth2/OIDC"
        apiGateway -> userService "Передает запросы по пользователям" "HTTPS/REST"
        apiGateway -> projectService "Передает запросы по проектам" "HTTPS/REST"
        apiGateway -> taskService "Передает запросы по задачам" "HTTPS/REST"

        userService -> userDB "Читает и записывает пользователей" "JDBC/SQL"
        projectService -> projectDB "Читает и записывает проекты" "JDBC/SQL"
        taskService -> taskDB "Читает и записывает задачи" "JDBC/SQL"

        userService -> cache "Кэширует пользователей по логину" "Redis Protocol"
        projectService -> cache "Кэширует проекты и список проектов" "Redis Protocol"

        taskService -> projectService "Проверяет проект и получает данные проекта" "HTTPS/REST"
        taskService -> userService "Проверяет исполнителя и получает данные пользователя" "HTTPS/REST"
        taskService -> vcsSystem "Получает связанные ветки, коммиты и pull request" "HTTPS/REST"

        userService -> messageQueue "Публикует событие UserCreated" "AMQP"
        projectService -> messageQueue "Публикует событие ProjectCreated" "AMQP"
        taskService -> messageQueue "Публикует события TaskCreated и TaskAssigned" "AMQP"
        messageQueue -> notificationService "Передает доменные события" "AMQP"

        notificationService -> emailSystem "Отправляет email-уведомления" "SMTP"
        notificationService -> messengerSystem "Отправляет уведомления в командные каналы" "HTTPS/REST"
    }

    views {
        systemContext projectManagementSystem "SystemContext" {
            include *
            autoLayout lr
            description "Диаграмма контекста системы управления проектами"
        }

        container projectManagementSystem "Container" {
            include *
            autoLayout lr
            description "Диаграмма контейнеров системы управления проектами"
        }

        dynamic projectManagementSystem "CreateTask" "Создание задачи в проекте" {
            projectManager -> webApp "1. Заполняет данные задачи и выбирает исполнителя"
            webApp -> apiGateway "2. POST /projects/{projectCode}/tasks"
            apiGateway -> authSystem "3. Проверить токен доступа"
            apiGateway -> taskService "4. Передать запрос на создание задачи"
            taskService -> projectService "5. Проверить проект"
            projectService -> projectDB "6. Получить данные проекта"
            taskService -> userService "7. Проверить исполнителя"
            userService -> userDB "8. Получить данные пользователя"
            taskService -> taskDB "9. Сохранить задачу"
            taskService -> messageQueue "10. Опубликовать события TaskCreated и TaskAssigned"
            messageQueue -> notificationService "11. Передать события"
            notificationService -> emailSystem "12. Отправить уведомление исполнителю"
            notificationService -> messengerSystem "13. Отправить уведомление в командный канал"
            autoLayout lr
            description "Последовательность создания задачи в проекте"
        }

        styles {
            element "Person" {
                shape Person
                background #08427b
                color #ffffff
            }
            element "Software System" {
                background #1168bd
                color #ffffff
            }
            element "Container" {
                background #438dd5
                color #ffffff
            }
            element "WebBrowser" {
                shape WebBrowser
            }
            element "API" {
                shape RoundedBox
                background #10b981
                color #ffffff
            }
            element "Service" {
                shape Hexagon
                background #438dd5
                color #ffffff
            }
            element "Database" {
                shape Cylinder
                background #438dd5
                color #ffffff
            }
            element "Cache" {
                shape Cylinder
                background #ff6b6b
                color #ffffff
            }
            element "Queue" {
                shape Pipe
                background #f59e0b
                color #ffffff
            }
        }

        theme default
    }
}
