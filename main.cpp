#include <iostream>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <fstream>


int seed;                       // Сид для псевдослучайности.
int start;                      // Начало дня.
std::istream* ins = &std::cin;  // Поток ввода
std::ostream* outs = &std::cout;// Поток вывода.

struct Seller {
    int id;                     // id продавца.

    std::string to_string() const {
        return "Seller #" + std::to_string(id + 1);
    }
} sellers[2];

struct Customer {
    int id;                     // id покупателя.
    int products_count;         // Количество продуктов в списке.
    Seller** products_from;     // Кассиры, продающие соответствующие продукты.

    std::string to_string() const {
        return "Customer #" + std::to_string(id + 1);
    }
};

Customer* customers;            // Покупатели за текущий день.
int customer_count;             // Количество покупателей.
Customer* last_in_queue[2];     // Последние покупати на i-ю кассу.
pthread_mutex_t mutex[2];       // mutex'ы для синхронизации потоков покупателей.
pthread_mutex_t out_mut;        // mutex для синхронизации вывода.
bool day;                       // Проверка на конец рабочего дня.

// Продажа товара.
void sell() {
    sleep(2);
}

// Функция для benchmark'а.
std::string time_attr() {
    return "[" + std::to_string((clock() - start) / (double)CLOCKS_PER_SEC) + "]: ";
}

// Моделирование очереди для кассира.
void* processSellerQueue(void* p_seller) {
    auto* seller = (Seller*)p_seller;
    while (day) {
        // Если очередь - пустая, то спим
        if (last_in_queue[seller->id] == nullptr) {
            continue;
        }

        // Начинаем продавать.
        pthread_mutex_lock(&out_mut);
        (*outs) << time_attr() << seller->to_string() << " started selling for "
                << last_in_queue[seller->id]->to_string() << '\n';
        pthread_mutex_unlock(&out_mut);

        sell();

        // Заканчиваем продавать.
        pthread_mutex_lock(&out_mut);
        (*outs) << time_attr() << seller->to_string() << " ended selling for "
                << last_in_queue[seller->id]->to_string() << '\n';
        pthread_mutex_unlock(&out_mut);

        // Говорим, что обслужили текущего покупателя.
        last_in_queue[seller->id] = nullptr;
    }
    return nullptr;
}

// Функция паралле
void* processPlan(void* p_customer) {
    auto* customer = (Customer*)p_customer;
    for (int i = 0; i < customer->products_count; i++) {
        int cur_seller = customer->products_from[i]->id;

        // Двигаемся на очередь на требуемую кассу.
        pthread_mutex_lock(&out_mut);
        (*outs) << time_attr() << customer->to_string() << " moved to "
                << customer->products_from[i]->to_string() << '\n';
        pthread_mutex_unlock(&out_mut);

        // С помощью mutex'ов, можно заблокировать доступ, если
        // потоком продавца уже обслуживается другой покупатель.
        pthread_mutex_lock(&mutex[cur_seller]);

        // Говорим, что текущий покупатель последний в очереди на кассу cur_seller.
        last_in_queue[cur_seller] = customer;

        // Ждем последнего в очереди покупателя.
        while (last_in_queue[cur_seller] != nullptr);
        pthread_mutex_unlock(&mutex[cur_seller]);
    }
    return nullptr;
}

int main(int argc, char **argv) {

    if (argc <= 1) {
        std::cout << "Wrong no. of arguments\n";
        return 0;
    }

    // Инициализация потоков и mutex'ов.
    pthread_t seller_threads[2];
    pthread_t* customer_threads;
    pthread_mutex_init(&out_mut, nullptr);

    for (int i = 0; i < 2; i++) {
        pthread_mutex_init(&mutex[i], nullptr);
        last_in_queue[i] = nullptr;
    }

    // Инициализация сущностей продавцов.
    for (int i = 0; i < 2; i++) {
        sellers[i] = Seller();
        sellers[i].id = i;
    }

    // Говорим, что рабочий день начался.
    day = true;
    //customer_count = 2;
    std::string type(argv[1]);
    if (type == "RANDOM") {              // Блок случайного ввода/вывода.
        seed = time(NULL);
        if (argc >= 3)
            seed = std::stoi(std::string(argv[2]));
        srand(seed);
        std::cout << "SEED: " << seed << '\n';
        customer_count = rand() % 10 + 1;
        customer_threads = new pthread_t[customer_count];
        customers = new Customer[customer_count];
        for (int i = 0; i < customer_count; i++) {
            customers[i] = Customer();
            customers[i].id = i;
            int count = 1 + rand() % 4;
            customers[i].products_count = count;
            customers[i].products_from = new Seller*[count];
            for (int j = 0; j < count; j++) {
                customers[i].products_from[j] = &sellers[rand() % 2];
            }
        }

        std::cout << "Customers:\n";
        for (int i = 0; i < customer_count; i++) {
            std::cout << i + 1 << ": ";
            for (int j = 0; j < customers[i].products_count; j++) {
                std::cout << customers[i].products_from[j]->id + 1 << ' ';
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    } else {
        if (type == "FILE_IO" && argc >= 4) {    // Блок файлового ввода.
            ins = new std::ifstream(argv[2]);
            outs = new std::ofstream(argv[3]);
        } else if (type != "CONSOLE_IO") {       // Блок консольного ввода.
            std::cout << "Wrong args.\n";
            return 0;
        }
        // Ввод количества покупателей.
        (*ins) >> customer_count;
        if (customer_count <= 0) {
            std::cout << "Incorrect no. of customers: " << customer_count << "\n";
            return 0;
        }
        customer_threads = new pthread_t[customer_count];
        customers = new Customer[customer_count];
        for (int i = 0; i < customer_count; i++) {
            // Ввод и создание i-го покупателя.
            customers[i] = Customer();
            customers[i].id = i;
            int count;
            (*ins) >> count;
            if (count <= 0) {
                std::cout << "Incorrect no. of products\n";
                return 0;
            }
            customers[i].products_count = count;
            customers[i].products_from = new Seller *[count];
            for (int j = 0; j < count; j++) {
                int id;
                (*ins) >> id;
                if (id != 1 && id != 2) {
                    std::cout << "Incorrect id of seller\n";
                    return 0;
                }
                customers[i].products_from[j] = &sellers[id - 1];
            }
        }
    }
    // Обновление начала дня.
    start = clock();

    // Создание потоков продавцов.
    for (int i = 0; i < 2; i++) {
        pthread_create(&seller_threads[i], nullptr, processSellerQueue, (void *) (sellers + i));
    }

    // Создание потоков покупателей.
    for (int i = 0; i < customer_count; i++) {
        pthread_create(&customer_threads[i], nullptr, processPlan, (void *) (customers + i));
    }

    // Синхронизируем потоки покупателей.
    for (int i = 0; i < customer_count; i++) {
        pthread_join(customer_threads[i], nullptr);
    }

    // Говорим, что рабочий день закончился.
    day = false;

    // Синхронизируем потоки продавцов.
    for (int i = 0; i < 2; i++) {
        pthread_join(seller_threads[i], nullptr);
    }

    // Уничтожение mutex'ов
    pthread_mutex_destroy(&out_mut);
    for (int i = 0; i < 2; i++) {
        pthread_mutex_destroy(&mutex[i]);
    }

    // Вывод всего записанного из файла.
    if (type == "FILE_IO") {
        std::ofstream* out = dynamic_cast<std::ofstream*>(outs);
        out->close();
        std::ifstream show(argv[3]);
        std::string s;
        while (std::getline(show, s)) {
            std::cout << s << '\n';
        }

        // Удаление потоков ввода/вывода.
        delete ins;
        delete outs;
    }
    return 0;
}
