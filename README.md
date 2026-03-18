# Order Execution Module

## 개요
Futures/Options 주문 체결 모듈입니다.
포지션 계산 결과를 입력받아 바이낸스에 주문을 전송합니다.

## 주요 기능
- Futures 매수/매도 주문
- Options 매수/매도 주문 (CALL/PUT)
- 다양한 예외 상황 처리

## 처리 케이스
1. 순차 주문: 심볼 A 포지션 계산 후 심볼 B 매수 주문
2. 동시 주문: 여러 심볼에 대한 동시 주문 처리
3. 충돌 주문: 매수 미체결 중 매도 주문 → 기존 주문 취소 후 새 주문 전송
4. 잔고 부족 → 주문 거절
5. 수량 0 이하 → 주문 거절
6. 옵션 필수 정보 누락 → 주문 거절
7. 존재하지 않는 주문 취소 → 실패 처리

## 빌드 및 실행
```bash
g++ -std=c++17 -o order_execution order_execution.cpp
./order_execution
```
