#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "order_execution.cpp"

// dummy data 파일에서 주문 읽어오기
std::vector<Order> load_orders(const std::string& filename) {
    std::vector<Order> orders;
    std::ifstream file(filename);
    std::string line;

    std::getline(file, line); // 헤더 스킵

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string symbol, side, type, quantity, limit_price;

        std::getline(ss, symbol,      ',');
        std::getline(ss, side,        ',');
        std::getline(ss, type,        ',');
        std::getline(ss, quantity,    ',');
        std::getline(ss, limit_price, ',');

        Order order;
        order.symbol          = symbol;
        order.side            = (side == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        order.instrument_type = InstrumentType::FUTURES;
        order.quantity        = std::stod(quantity);
        order.price           = std::stod(limit_price);

        orders.push_back(order);
    }
    return orders;
}

int main(int argc, char* argv[]) {
    Portfolio portfolio;
    portfolio.cash         = 100000.0;
    portfolio.realized_pnl = 0.0;

    OrderExecutor executor(portfolio);

    std::string filename = "dummy_data/single_tick.txt"; // 기본값
    if (argc > 1) filename = argv[1];                    // 인자로 파일 지정 가능

    std::cout << "파일 로드: " << filename << std::endl;
    std::vector<Order> orders = load_orders(filename);
    std::cout << "총 " << orders.size() << "개 주문 로드됨\n" << std::endl;

    for (auto& order : orders) {
        executor.executeOrder(order);
        executor.fillOrder(order.symbol);
    }

    executor.printPortfolio();
    return 0;
}