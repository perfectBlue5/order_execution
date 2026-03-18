#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

// ── 주문 방향 ──────────────────────────────────────────────
enum class OrderSide {
    BUY,
    SELL
};

// ── 상품 타입 ──────────────────────────────────────────────
enum class InstrumentType {
    FUTURES,
    OPTIONS
};

// ── 주문 상태 ──────────────────────────────────────────────
enum class OrderStatus {
    PENDING,    // 거래소에 전송됨, 아직 미체결
    FILLED,     // 체결 완료
    CANCELED,   // 취소됨
    REJECTED    // 거절됨
};

// ── 포지션 ────────────────────────────────────────────────
struct Position {
    std::string symbol;
    double      quantity      = 0.0;
    double      entry_price   = 0.0;
    double      current_price = 0.0;

    double unrealized_pnl() const {
        return quantity * (current_price - entry_price);
    }
};

// ── 주문 ──────────────────────────────────────────────────
struct Order {
    int            id             = 0;
    std::string    symbol;
    OrderSide      side;
    InstrumentType instrument_type;
    OrderStatus    status         = OrderStatus::PENDING;
    double         quantity       = 0.0;
    double         price          = 0.0;

    // 옵션 전용
    std::string    option_type;
    double         strike_price   = 0.0;
    std::string    expiry;
};

// ── 포트폴리오 ────────────────────────────────────────────
struct Portfolio {
    double                                    cash          = 0.0;
    double                                    realized_pnl  = 0.0;
    std::unordered_map<std::string, Position> positions;

    double total_value() const {
        double unrealized = 0.0;
        for (const auto& [sym, pos] : positions)
            unrealized += pos.unrealized_pnl();
        return cash + unrealized + realized_pnl;
    }
};

// ── OrderExecutor ─────────────────────────────────────────
class OrderExecutor {
public:
    OrderExecutor(Portfolio& portfolio) : portfolio_(portfolio) {}

    // 단일 주문 실행
    bool executeOrder(Order& order) {
        order.id = ++next_id_;
        std::cout << "\n[주문 접수] ID=" << order.id
                  << " | " << order.symbol
                  << " | " << (order.side == OrderSide::BUY ? "매수" : "매도")
                  << " | " << (order.instrument_type == InstrumentType::FUTURES ? "Futures" : "Options")
                  << std::endl;

        if (!validateOrder(order))  return false;
        if (!checkMargin(order))    return false;
        if (!resolveConflict(order)) return false;

        sendOrder(order);
        return true;
    }

    // 여러 주문 동시 실행 (케이스 2)
    void executeOrders(std::vector<Order>& orders) {
        std::cout << "\n[동시 주문] " << orders.size() << "개 주문 실행" << std::endl;
        for (auto& order : orders)
            executeOrder(order);
    }

    // 주문 수동 체결 (테스트용 - 실제로는 거래소가 체결 통보)
    bool fillOrder(const std::string& symbol) {
        auto it = pending_orders_.find(symbol);
        if (it == pending_orders_.end()) {
            std::cout << "[체결 실패] " << symbol << " 미체결 주문 없음" << std::endl;
            return false;
        }
        Order order = it->second; // 복사본 만들기
        pending_orders_.erase(it); // 먼저 지우고
        simulateFill(order);       // 복사본으로 체결
        return true;
    }

    // 주문 취소
    bool cancelOrder(int order_id) {
        for (auto it = pending_orders_.begin(); it != pending_orders_.end(); ++it) {
            if (it->second.id == order_id) {
                if (it->second.status == OrderStatus::FILLED) {
                    std::cout << "[취소 실패] ID=" << order_id << " 이미 체결된 주문" << std::endl;
                    return false;
                }
                std::cout << "[취소 완료] ID=" << order_id
                          << " | " << it->second.symbol << std::endl;
                it->second.status = OrderStatus::CANCELED;
                pending_orders_.erase(it);
                return true;
            }
        }
        std::cout << "[취소 실패] ID=" << order_id << " 주문을 찾을 수 없음" << std::endl;
        return false;
    }

    // 포트폴리오 현황 출력
    void printPortfolio() const {
        std::cout << "\n── 포트폴리오 현황 ──────────────────" << std::endl;
        std::cout << "  Cash:         " << portfolio_.cash << std::endl;
        std::cout << "  Realized PnL: " << portfolio_.realized_pnl << std::endl;
        for (const auto& [sym, pos] : portfolio_.positions) {
            std::cout << "  포지션 [" << sym << "]"
                      << " 수량=" << pos.quantity
                      << " | 진입가=" << pos.entry_price
                      << " | 현재가=" << pos.current_price
                      << " | 미실현손익=" << pos.unrealized_pnl()
                      << std::endl;
        }
        std::cout << "  Total Equity: " << portfolio_.total_value() << std::endl;

        if (!pending_orders_.empty()) {
            std::cout << "  미체결 주문:" << std::endl;
            for (const auto& [sym, ord] : pending_orders_) {
                std::cout << "    [" << sym << "] ID=" << ord.id
                          << " | " << (ord.side == OrderSide::BUY ? "매수" : "매도")
                          << " | 수량=" << ord.quantity
                          << " | 상태=PENDING" << std::endl;
            }
        }
        std::cout << "────────────────────────────────────" << std::endl;
    }

private:
    Portfolio&                             portfolio_;
    std::unordered_map<std::string, Order> pending_orders_; // 심볼별 미체결 주문
    int                                    next_id_ = 0;

    // ── 1. 유효성 검사 ───────────────────────────────────
    bool validateOrder(Order& order) {
        if (order.quantity <= 0) {
            reject(order, "수량이 0 이하");
            return false;
        }
        if (order.price < 0) {
            reject(order, "가격이 음수");
            return false;
        }
        if (order.instrument_type == InstrumentType::OPTIONS) {
            if (order.option_type != "CALL" && order.option_type != "PUT") {
                reject(order, "옵션 타입 누락 또는 잘못됨 (CALL/PUT)");
                return false;
            }
            if (order.strike_price <= 0) {
                reject(order, "옵션 행사가격 누락");
                return false;
            }
            if (order.expiry.empty()) {
                reject(order, "옵션 만료일 누락");
                return false;
            }
        }
        return true;
    }

    // ── 2. 잔고 확인 ─────────────────────────────────────
    bool checkMargin(Order& order) {
        if (order.side == OrderSide::BUY) {
            double required = order.quantity * order.price;
            if (portfolio_.cash < required) {
                reject(order, "잔고 부족 (필요: " + std::to_string(required)
                              + ", 보유: " + std::to_string(portfolio_.cash) + ")");
                return false;
            }
        }
        return true;
    }

    // ── 3. 충돌 주문 처리 ────────────────────────────────
    bool resolveConflict(Order& order) {
        auto it = pending_orders_.find(order.symbol);
        if (it == pending_orders_.end()) return true;

        Order& existing = it->second;

        // 같은 방향 미체결 주문 있는 경우
        if (existing.side == order.side) {
            std::cout << "[경고] " << order.symbol
                      << " 동일 방향 미체결 주문 존재 (ID=" << existing.id
                      << "). 기존 주문 취소 후 새 주문 진행." << std::endl;
            existing.status = OrderStatus::CANCELED;
            pending_orders_.erase(it);
            return true;
        }

        // 반대 방향 미체결 주문 있는 경우 (핵심: 케이스 3)
        std::cout << "[충돌 감지] " << order.symbol
                  << " 반대 방향 미체결 주문 존재 (ID=" << existing.id
                  << ", " << (existing.side == OrderSide::BUY ? "매수" : "매도") << ")"
                  << std::endl;
        std::cout << "  → 기존 주문 취소 요청 중..." << std::endl;

        existing.status = OrderStatus::CANCELED;
        pending_orders_.erase(it);

        std::cout << "  → 취소 완료. 새 주문 진행." << std::endl;
        return true;
    }

    // ── 4. 주문 전송 (PENDING 상태로 등록) ───────────────
    void sendOrder(Order& order) {
        order.status = OrderStatus::PENDING;
        pending_orders_[order.symbol] = order;

        if (order.instrument_type == InstrumentType::FUTURES) {
            std::cout << "[Futures 전송] "
                      << (order.side == OrderSide::BUY ? "매수" : "매도")
                      << " | " << order.symbol
                      << " | 수량=" << order.quantity
                      << " | 가격=" << order.price
                      << " → 바이낸스 (PENDING)" << std::endl;
        } else {
            std::cout << "[Options 전송] "
                      << (order.side == OrderSide::BUY ? "매수" : "매도")
                      << " | " << order.symbol
                      << " | 타입=" << order.option_type
                      << " | 행사가=" << order.strike_price
                      << " | 만료일=" << order.expiry
                      << " | 수량=" << order.quantity
                      << " → 바이낸스 (PENDING)" << std::endl;
        }
    }

    // ── 5. 체결 처리 ─────────────────────────────────────
    void simulateFill(Order& order) {
        order.status = OrderStatus::FILLED;

        if (order.instrument_type == InstrumentType::FUTURES) {
            if (order.side == OrderSide::BUY) {
                auto& pos         = portfolio_.positions[order.symbol];
                pos.symbol        = order.symbol;
                pos.quantity      += order.quantity;
                pos.entry_price   = order.price;
                pos.current_price = order.price;
                portfolio_.cash   -= order.quantity * order.price;
            } else {
                auto it = portfolio_.positions.find(order.symbol);
                if (it != portfolio_.positions.end()) {
                    double pnl           = order.quantity * (order.price - it->second.entry_price);
                    portfolio_.realized_pnl += pnl;
                    portfolio_.cash         += order.quantity * order.price;
                    it->second.quantity     -= order.quantity;
                    if (it->second.quantity <= 0)
                        portfolio_.positions.erase(it);
                }
            }
        }

    
        std::cout << "[체결 완료] ID=" << order.id
                  << " | " << order.symbol
                  << " | 가격=" << order.price << std::endl;
    }

    // ── 주문 거절 ────────────────────────────────────────
    void reject(Order& order, const std::string& reason) {
        order.status = OrderStatus::REJECTED;
        std::cout << "[거절] ID=" << order.id
                  << " | " << order.symbol
                  << " | 이유: " << reason << std::endl;
    }
};

// ═════════════════════════════════════════════════════════
