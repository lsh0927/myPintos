**PintOS Project - Main Branch**

---

## 📌 Project Overview
본 프로젝트는 **PintOS 운영체제**의 스레드(Thread) 부분을 구현하고 테스트한 결과를 정리한 것입니다.  
구현한 기능은 다음과 같습니다:

- **Threads**
  - Alarm Clock
  - Priority Scheduling
  - Advanced Scheduler

---

## ✅ Test Results

### 🔔 Alarm Clock
| Test | Result |
|------|--------|
| alarm-single       | ✅ PASS |
| alarm-multiple     | ✅ PASS |
| alarm-simultaneous | ✅ PASS |
| alarm-priority     | ✅ PASS |
| alarm-zero         | ✅ PASS |
| alarm-negative     | ✅ PASS |

### ⚡ Priority Scheduling
| Test | Result |
|------|--------|
| priority-change          | ✅ PASS |
| priority-donate-one      | ✅ PASS |
| priority-donate-multiple | ✅ PASS |
| priority-donate-multiple2| ✅ PASS |
| priority-donate-nest     | ❌ FAIL |
| priority-donate-sema     | ❌ FAIL |
| priority-donate-lower    | ✅ PASS |
| priority-fifo            | ✅ PASS |
| priority-preempt         | ✅ PASS |
| priority-sema            | ✅ PASS |
| priority-condvar         | ❌ FAIL |
| priority-donate-chain    | ❌ FAIL |

### 📊 Advanced Scheduler (MLFQS)
| Test | Result |
|------|--------|
| mlfqs-load-1   | 🚧 NONE |
| mlfqs-load-60  | 🚧 NONE |
| mlfqs-load-avg | 🚧 NONE |
| mlfqs-recent-1 | 🚧 NONE |
| mlfqs-fair-2   | 🚧 NONE |
| mlfqs-fair-20  | 🚧 NONE |
| mlfqs-nice-2   | 🚧 NONE |
| mlfqs-nice-10  | 🚧 NONE |

---

## 📂 Summary
- **Alarm Clock**: 모든 테스트 **성공** 🎉  
- **Priority Scheduling**: 일부 테스트 **실패(4개)**  
- **Advanced Scheduler**: 아직 구현하지 않음 🚧  

---

## 📝 Notes
- `priority-donate-nest`, `priority-donate-sema`, `priority-condvar`, `priority-donate-chain` 테스트에서 FAIL 발생  
- Advanced Scheduler(MLFQS) 기능은 추후 구현 예정