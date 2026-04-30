# Lộ Trình Huấn Luyện AI (Curriculum Learning)

Quá trình huấn luyện AI cho AZTank sử dụng phương pháp **Curriculum Learning (Học theo giáo trình từ dễ đến khó)** kết hợp với **Self-play (Tự đấu với chính phiên bản cũ của mình)**. Dưới đây là bảng tóm tắt mục đích của 7 giai đoạn (Phase) huấn luyện:

| Giai đoạn | Số Bước (Steps) | Có Tường (Map) | Vật Phẩm (Items) | Đối thủ (Opponent) | Mục đích huấn luyện chính |
| :---: | :--- | :---: | :---: | :--- | :--- |
| **Phase 1** | 500,000 | ❌ Không | ❌ Không | Bia đứng im (None) | **Học kỹ năng cơ bản:** Giúp Agent làm quen với cách xoay xe, di chuyển thẳng, khai hỏa và ngắm trúng mục tiêu cố định trong một bãi đất trống. |
| **Phase 2** | 1,000,000 | ✅ Có | ❌ Không | Bia đứng im (None) | **Học né tường và tìm đường:** Đưa mê cung vào. Agent phải học cách di chuyển trong không gian hẹp, sử dụng định tuyến A* để len lỏi qua các bức tường tìm đến bia ngắm mà không bị kẹt. |
| **Phase 3** | 1,500,000 | ❌ Không | ❌ Không | AI từ Phase 2 | **Học chiến đấu tay đôi (Địa hình trống):** Bỏ tường đi, nhưng đối thủ giờ là một AI đã biết di chuyển và bắn. Agent học cách "Dogfight", né đạn đang bay tới và bắn trúng mục tiêu di động. |
| **Phase 4** | 2,000,000 | ✅ Có | ❌ Không | AI từ Phase 3 | **Chiến đấu trong mê cung (Cơ bản):** Gộp kỹ năng của Phase 2 và Phase 3. Agent phải vừa chiến đấu với kẻ thù biết di chuyển, vừa phải lợi dụng các vách tường để nấp hoặc làm đạn nảy. |
| **Phase 5** | 2,000,000 | ✅ Có | ❌ Không | AI từ Phase 4 | **Hoàn thiện chiến thuật (Nâng cao):** Self-play với một AI đã biết lợi dụng mê cung. Ép Agent phải học các chiến thuật tinh vi hơn như: bắn nhử, chạy trốn khi cooldown, bắn lén qua nảy tường. |
| **Phase 6** | 2,000,000 | ✅ Có | ✅ Có | AI từ Phase 5 | **Làm quen với Vật phẩm:** Kích hoạt các hộp vật phẩm (nếu có logic). Agent bắt đầu học thêm thói quen tranh giành Item để lấy lợi thế thay vì chỉ tập trung vào việc bắn nhau. |
| **Phase 7** | 3,000,000 | ✅ Có | ✅ Có | AI từ Phase 6 | **Đạt cấp độ Master (Hoàn thiện):** Giai đoạn dài nhất để Agent đánh bóng lại toàn bộ các kỹ năng (di chuyển, nấp tường, nhặt item, bắn nảy đạn, né đạn) với một đối thủ rất thông minh. |

### Nguyên lý thiết kế
Cách thiết kế 7 Phase này đi theo một lộ trình sư phạm rất chuẩn mực của Reinforcement Learning:
* Môi trường từ Đơn giản $\rightarrow$ Phức tạp (Không tường $\rightarrow$ Có tường $\rightarrow$ Có Items).
* Kẻ thù từ Thụ động $\rightarrow$ Tinh nhuệ (Đứng im $\rightarrow$ Biết đi $\rightarrow$ Biết chiến đấu $\rightarrow$ Chiến đấu Pro).
