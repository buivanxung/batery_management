# DỰ ÁN POWER BANKING - KHOÁ THANH TOÁN (PAYMENT SCHEDULE)

**Tổng chi phí dự án:** 452,000,000 VND  
**Thời gian dự án:** ~250 ngày (5-6 tháng)

---

## 1. PHƯƠNG ÁN THANH TOÁN - 5 ĐỢT

### Phương Án A: Chia 5 Đợt Đều (Khuyến Nghị)

| Đợt | Tên Gọi | Thời Điểm | % Chi Phí | Số Tiền (VND) | Điều Kiện Thanh Toán |
|-----|---------|-----------|----------|---------------|----------------------|
| **1** | Hợp đồng & Kick-off | Ký hợp đồng | 20% | 90,400,000 | Ký kết hợp đồng, kick-off meeting |
| **2** | Hardware + Backend v1 | Tuần 8 | 20% | 90,400,000 | Hardware prototype hoàn thành, Backend APIs v1 ready |
| **3** | Frontend hoàn thành | Tuần 14 | 20% | 90,400,000 | Frontend Owner + User hoàn thành, QA riêng pass |
| **4** | QA & UAT | Tuần 19 | 20% | 90,400,000 | Full QA pass, UAT completed, production ready |
| **5** | Go-live & Warranty | Tuần 20 | 20% | 90,400,000 | Production deployment, 3 tháng support/warranty |
| | | | **TỔNG** | **452,000,000** | |

**✓ Lợi ích phương án 5 đợt đều:**
- Rủi ro chia đều cho cả 2 bên
- Payment flow ổn định, dễ quản lý ngân sách
- Các milestone rõ ràng, dễ đo lường tiến độ

---

## 2. CHI TIẾT TỪNG ĐỢT THANH TOÁN

### **ĐỢT 1: KICK-OFF & HỢP ĐỒNG (20%)**
**Số tiền:** 90,400,000 VND  
**Thời điểm:** Ngay khi ký hợp đồng  
**Điều kiện thanh toán:**
- [ ] Hợp đồng được ký kết bởi cả 2 bên
- [ ] Kick-off meeting diễn ra
- [ ] Team được xác nhận & bắt đầu làm việc
- [ ] Project plan được phê duyệt

**Công việc sẽ bắt đầu:**
- Thiết kế hệ thống toàn bộ
- Chuẩn bị environments (dev, staging, prod)
- Design database schema
- Design UI/UX mockups

---

### **ĐỢT 2: MILESTONE 1 - HARDWARE + BACKEND v1 (20%)**
**Số tiền:** 90,400,000 VND  
**Thời điểm:** Tuần thứ 8 (khoảng 2 tháng)  
**Điều kiện thanh toán:**

#### Hardware:
- [ ] PCB design hoàn thành, đã ordered
- [ ] Firmware ESP32 cơ bản hoàn thành
  - ✓ 4G connection & MQTT pub/sub working
  - ✓ 8 GPIO relay control working
  - ✓ OTA update mechanism ready
- [ ] Device prototype có thể điều khiển từ server
- [ ] Hardware burn-in test started

#### Backend:
- [ ] Database schema deployed (PostgreSQL production)
- [ ] Core APIs implemented:
  - ✓ User Management API (register, login, JWT)
  - ✓ Device Management API (list, create, update)
  - ✓ Device Status API (real-time via MQTT)
  - ✓ Basic Transaction API
- [ ] MQTT Broker configured & running
- [ ] API Documentation (Swagger/Postman)
- [ ] Unit tests pass (>70% coverage)
- [ ] Staging environment deployed & accessible

**Deliverables:**
- Hardware prototype (1-2 units)
- Postman collection with all APIs
- Database backup
- Deployment guide
- Test reports

---

### **ĐỢT 3: MILESTONE 2 - FRONTEND + QA (20%)**
**Số tiền:** 90,400,000 VND  
**Thời điểm:** Tuần thứ 14 (khoảng 3.5 tháng)  
**Điều kiện thanh toán:**

#### Frontend Owner (Chủ Device):
- [ ] Dashboard hoàn thành 100%
  - ✓ Login/Register/Profile
  - ✓ Device list & control
  - ✓ Set charging time per slot
  - ✓ Transaction history
  - ✓ Revenue analytics
- [ ] Mobile responsive working
- [ ] Notifications (email/SMS) working
- [ ] Deployed on staging

#### Frontend User (Người dùng):
- [ ] App hoàn thành 100%
  - ✓ Registration & KYC
  - ✓ Device search (map + filters)
  - ✓ Rental flow (select → checkout)
  - ✓ Momo payment integration
  - ✓ Timer & time tracking
  - ✓ Transaction history
- [ ] QR code scanner working
- [ ] Push notifications working
- [ ] Deployed on staging

#### QA:
- [ ] Functional testing completed
  - ✓ All features tested
  - ✓ Edge cases covered
  - ✓ Error handling verified
- [ ] Performance baseline established
- [ ] Security scan done
- [ ] QA report pass rate >95%

**Deliverables:**
- Frontend source code (with tests)
- Deployment scripts
- API integration documentation
- QA test cases & reports
- Known issues (if any) log

---

### **ĐỢT 4: MILESTONE 3 - QA + UAT + OPTIMIZATION (20%)**
**Số tiền:** 90,400,000 VND  
**Thời điểm:** Tuần thứ 19 (khoảng 4.5 tháng)  
**Điều kiện thanh toán:**

#### Full System Testing:
- [ ] Integration testing (Hardware + Backend + Frontend)
- [ ] Load testing with 1000+ concurrent users
- [ ] All critical bugs fixed
- [ ] Performance optimization completed
- [ ] Security audit passed

#### User Acceptance Testing (UAT):
- [ ] UAT with sample users (Owner + Users) - Min 50 test cases
- [ ] UAT test pass rate ≥ 95%
- [ ] User feedback addressed
- [ ] Production checklist completed

#### Production Readiness:
- [ ] Database auto-backup configured
- [ ] Monitoring & alerting setup (Grafana/CloudWatch)
- [ ] Logging system ready (ELK/CloudWatch)
- [ ] Disaster recovery plan documented
- [ ] Scaling plan documented
- [ ] SLA defined & communicated

#### Optimization:
- [ ] Backend APIs optimized (response time < 200ms)
- [ ] Frontend load time optimized (< 3s)
- [ ] Database queries optimized
- [ ] CDN configured (if needed)

**Deliverables:**
- UAT sign-off document
- Production deployment guide
- Operations manual
- Monitoring dashboard access
- Incident response playbook
- Performance benchmark report

---

### **ĐỢT 5: GO-LIVE + SUPPORT (20%)**
**Số tiền:** 90,400,000 VND  
**Thời điểm:** Tuần thứ 20 & sau đó  
**Điều kiện thanh toán:**

#### Go-Live:
- [ ] Production environment fully deployed
- [ ] All hardware units deployed at locations
- [ ] All users able to access system
- [ ] Customer support team trained
- [ ] Marketing materials ready

#### Warranty & Support (3 months):
- [ ] 24/7 production monitoring
- [ ] Critical bugs fixed within 24h
- [ ] Minor bugs fixed within 1 week
- [ ] Performance issues resolved
- [ ] Security patches applied
- [ ] Monthly health check report provided
- [ ] User training sessions conducted

**Deliverables:**
- Production deployment completion report
- Monitoring dashboard (24/7 access)
- Weekly status reports
- Support ticket system setup
- Monthly optimization recommendations

---

## 3. CHI TIẾT VỀ TỪNG THÀNH PHẦN

### Phân Bổ Chi Phí Theo Đợt

```
ĐỢT 1 (20%):
├─ PM & Design: 10,000,000
├─ Database Setup: 3,000,000
├─ Infrastructure Setup: 5,000,000
└─ Hardware Design: 8,000,000

ĐỢT 2 (20%):
├─ Hardware Dev: 20,000,000
├─ Backend Dev: 50,000,000
├─ Infrastructure: 10,000,000
└─ Testing: 10,400,000

ĐỢT 3 (20%):
├─ Frontend Owner: 40,000,000
├─ Frontend User: 45,000,000
└─ QA Initial: 5,400,000

ĐỢT 4 (20%):
├─ Full QA: 30,000,000
├─ UAT: 50,000,000
└─ Optimization: 10,400,000

ĐỢT 5 (20%):
├─ Deployment: 30,000,000
├─ Training: 20,000,000
├─ Support (3 tháng): 35,000,000
└─ Documentation: 5,400,000
```

---

## 4. PHƯƠNG ÁN THANH TOÁN KHÁC (TÙY CHỌN)

### Phương Án B: Chia 4 Đợt (Aggressive)

| Đợt | Thời Điểm | % | Số Tiền | Điều Kiện |
|-----|-----------|---|--------|----------|
| 1 | Hợp đồng ký | 30% | 135,600,000 | Ký hợp đồng + kick-off |
| 2 | Tuần 10 | 25% | 113,000,000 | Hardware + Backend hoàn thành |
| 3 | Tuần 16 | 25% | 113,000,000 | Frontend hoàn thành + QA |
| 4 | Go-live | 20% | 90,400,000 | Production deployment |
| | | **100%** | **452,000,000** | |

**Note:** Đợt 1 cao hơn, rủi ro cho nhà phát triển lớn hơn

---

### Phương Án C: Chia 6 Đợt (Conservative)

| Đợt | Thời Điểm | % | Số Tiền | Điều Kiện |
|-----|-----------|---|--------|----------|
| 1 | Hợp đồng | 15% | 67,800,000 | Ký hợp đồng |
| 2 | Tuần 7 | 15% | 67,800,000 | Design + Infrastructure |
| 3 | Tuần 10 | 18% | 81,360,000 | Hardware + Backend v1 |
| 4 | Tuần 14 | 18% | 81,360,000 | Frontend hoàn thành |
| 5 | Tuần 18 | 19% | 85,880,000 | QA pass + UAT |
| 6 | Tuần 21 | 15% | 67,800,000 | Go-live + 3 tháng support |
| | | **100%** | **452,000,000** | |

**Note:** Rủi ro chia đều hơn, nhưng cash flow nhiều hơn

---

## 5. ĐIỀU KHOẢN ĐẶC BIỆT

### Nếu Tiến Độ Muộn:
- **Chậm 1-2 tuần:** Không ảnh hưởng, extend deadline
- **Chậm > 2 tuần:** 
  - Giảm 5% đợt thanh toán tiếp theo
  - Hoặc extend timeline, không giảm giá
  - Bàn bạc lại (negotiate)

### Nếu Yêu Cầu Thay Đổi (Change Request):
- **Scope nhỏ (< 5 ngày):** Miễn phí
- **Scope trung bình (5-15 ngày):** Tính thêm theo ngày công
- **Scope lớn (> 15 ngày):** Bàn bạc lại hợp đồng, có thể delay timeline

### Thanh Toán Lớn Hơn (Acceleration):
Nếu muốn hoàn thành nhanh hơn, các đợt có thể:
- Gộp nhân viên thêm: +20% chi phí
- Overtime: +15% chi phí
- Fast-track: +25-30% chi phí

---

## 6. PHƯƠNG THỨC THANH TOÁN

### Ngân Hàng:
- **Tên:** [Chi tiết ngân hàng nhà phát triển]
- **Số tài khoản:** [Số tài khoản]
- **Chi nhánh:** [Tên chi nhánh]
- **Swift Code:** [Code nếu cần]

### Phương Thức:
- ✓ Chuyển khoản (Bank transfer) - **Ưu tiên**
- ✓ Check
- ✓ Tiền mặt (với hóa đơn)

### Hóa Đơn:
- Hóa đơn được phát theo từng đợt thanh toán
- Hóa đơn phải được thanh toán trong vòng 5 ngày làm việc sau khi phát hành
- Nếu chậm trên 30 ngày: 1% phí trả chậm mỗi tháng

---

## 7. SO SÁNH CÁC PHƯƠNG ÁN

| Tiêu Chí | Phương Án A (5 đợt) | Phương Án B (4 đợt) | Phương Án C (6 đợt) |
|----------|-------------------|-------------------|-------------------|
| **Rủi ro cho chủ** | Thấp | Cao | Rất thấp |
| **Rủi ro cho dev** | Trung bình | Thấp | Cao |
| **Dễ quản lý** | ✓✓ Rất tốt | ✓ Tốt | ✓ Tốt |
| **Cash flow** | Đều đặn | Sớm hơn | Liên tục |
| **Flexibility** | Cao | Trung bình | Thấp |
| **Khuyến nghị** | **BEST** | Nếu dev có uy tín | Nếu chủ còn nghi ngờ |

---

## 8. TIMELINE CỤ THỂ

### Phương Án A - Timeline 6 Tuần (Đối với đợt 1-5)

```
TUẦN 1-2   Phân tích, design, kick-off
│          [ĐỢT 1 THANH TOÁN ✓]
│
TUẦN 3-6   Hardware test, Backend APIs
│
TUẦN 7-8   ✓ Milestone 1
           [ĐỢT 2 THANH TOÁN ✓]
           
TUẦN 9-12  Frontend development
│
TUẦN 13-14 Frontend QA
           [ĐỢT 3 THANH TOÁN ✓]
│
TUẦN 15-18 Full system testing, UAT
│
TUẦN 19    ✓ Milestone 3
           [ĐỢT 4 THANH TOÁN ✓]
│
TUẦN 20    Production deployment
           [ĐỢT 5 THANH TOÁN ✓]
│
TUẦN 21-24 Support + Warranty
```

---

## 9. TỔNG HỢP TIMELINE & THANH TOÁN

| Tuần | Công Việc Chính | Điểm Checkup | Thanh Toán |
|-----|-----------------|-------------|-----------|
| W1-2 | Design + Kick-off | ✓ Planning done | **ĐỢT 1** (90.4M) |
| W3-6 | Hardware + Backend | 50% done | - |
| W7-8 | Hardware Ready | ✓ Milestone 1 | **ĐỢT 2** (90.4M) |
| W9-12 | Frontend Dev | 70% done | - |
| W13-14 | Frontend QA | ✓ Milestone 2 | **ĐỢT 3** (90.4M) |
| W15-18 | Full Testing + UAT | 90% done | - |
| W19 | Go-live Prep | ✓ Milestone 3 | **ĐỢT 4** (90.4M) |
| W20-24 | Production + Support | ✓ Live | **ĐỢT 5** (90.4M) |

---

## 10. RISK & CONTINGENCY

### Nếu Vượt Chi Phí:
- Budget reserve: 10% (45.2M VND)
- Giải quyết: 
  - Scope reduction
  - Timeline extension
  - Thêm funding
  - Renegotiate

### Nếu Chậm Tiến Độ:
- Extend các milestone: +1-2 tuần mỗi milestone
- Không tính phí nếu do vấn đề bên ngoài (4G outage, etc.)
- Tính phí nếu do team (delayed decisions, late feedback)

---

**Lưu ý:** Tài liệu này cần được confirming bởi cả 2 bên trước khi ký hợp đồng chính thức.

*Cập nhật: April 7, 2026*
