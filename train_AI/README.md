# 🤖 Hướng dẫn Huấn luyện AI (Reinforcement Learning)

Thư mục này chứa toàn bộ mã nguồn và cấu hình để huấn luyện AI cho AZ Tank Game sử dụng thuật toán **PPO (Stable-Baselines3)**. Nhờ cấu trúc *Curriculum Learning 10 Giai đoạn*, bất kỳ ai trong nhóm cũng có thể tiếp tục kế thừa model hiện tại và huấn luyện AI thông minh hơn.

---

## 📦 Yêu cầu cài đặt (Prerequisites)

1. Cần chắc chắn bạn đã biên dịch thành công phần lõi C++ (file `.pyd` trên Windows hoặc `.so` trên Linux) qua thư mục `build` ở ngoài thư mục gốc, và file thư viện đã có thể nhận diện bởi Python.
2. Cài đặt các thư viện Python cần thiết:
   ```bash
   pip install stable-baselines3 gymnasium numpy torch tensorboard
   ```

---

## 🚀 Cách tiếp tục huấn luyện (Resume Training)

Dự án hiện tại đã huấn luyện sẵn các model từ **Phase 1 đến Phase 9** (lưu tại thư mục `train_AI/models/`). 

Để tiếp tục huấn luyện từ Phase 9 (hoặc chuyển sang Phase 10 - Self-Play), bạn mở Terminal/PowerShell, chuyển hướng vào thư mục `train_AI` và chạy lệnh sau:

```bash
cd train_AI
python train_ai.py --pipeline --phase 9 --resume
```

**Giải thích lệnh:**
*   `--pipeline`: Chạy hệ thống chuyển cấp tự động. Nếu xong Phase 9 nó sẽ tự sang Phase 10.
*   `--phase 9`: Bắt đầu tiến trình từ Phase 9.
*   `--resume`: Tải lại file `models/ppo_tank_phase9.zip` đã có sẵn trên GitHub để học tiếp thay vì tạo mới từ đầu.

Nếu bạn chỉ muốn train **duy nhất** Phase 10 (bỏ qua pipeline):
```bash
python train_ai.py --phase 10
```

---

## 📊 Theo dõi số liệu huấn luyện (TensorBoard)

Trong quá trình AI đang học, bạn có thể xem biểu đồ thời gian thực (điểm thưởng Reward, độ dài trận đấu, Loss) bằng TensorBoard.

1. Mở một cửa sổ Terminal/PowerShell mới.
2. Di chuyển vào thư mục `train_AI` và khởi động TensorBoard:
   ```bash
   cd train_AI
   tensorboard --logdir ./logs/ --host 127.0.0.1
   ```
   *(Lưu ý: Nếu bị lỗi command not found, hãy dùng `python -m tensorboard.main --logdir ./logs/ --host 127.0.0.1`)*
3. Mở trình duyệt web và truy cập: 👉 **http://127.0.0.1:6006**

---

## 🖥️ Huấn luyện bằng CPU hay GPU?

Mô hình trong dự án này được thiết kế để tối ưu hóa cực tốt khi chạy trên **CPU**. Bạn **KHÔNG CẦN** và không nên cấu hình train bằng GPU vì các lý do sau:

1. **Mạng Nơ-ron rất nhỏ**: PPO sử dụng mạng MlpPolicy với đầu vào chỉ là một vector 45 chiều (các góc bắn, khoảng cách). Tầng ẩn rất nhỏ nên CPU tính toán chỉ mất vài micro giây.
2. **Nghẽn cổ chai truyền dữ liệu**: Lõi vật lý Box2D chạy trên CPU. Nếu train bằng GPU, mỗi bước đi (step) của game sẽ bắt hệ thống phải copy dữ liệu qua lại giữa RAM (CPU) và VRAM (GPU). Việc truyền dữ liệu này tốn nhiều thời gian hơn cả việc tính toán, làm tốc độ train chậm đi đáng kể.
3. **Song song hóa**: Môi trường đã được cấu hình chạy song song 4 trận đấu (`n_envs=4`), sử dụng đa luồng C++ ngầm rất hiệu quả trên CPU.

---

## 💾 Cấu trúc thư mục Models

- `models/`: Nơi lưu các model chính thức (ví dụ: `ppo_tank_phase9.zip`). Đây là những file nhỏ, gọn và đã được theo dõi (track) trên GitHub.
- `models/checkpoints/`: Nơi tự động lưu trữ các bản nháp sau mỗi 50.000 bước. Rất hữu ích để cứu nguy nếu máy bạn bị sập nguồn giữa chừng. Thư mục này thường rất nặng nên đã được đưa vào `.gitignore` để tránh đầy dung lượng kho lưu trữ.

---

## 🎮 Xem AI chiến đấu (Test Model)

Bất cứ lúc nào bạn cũng có thể mở giao diện đồ họa lên để xem AI hiện tại đang "múa" thế nào trước các Bot C++:

```bash
cd train_AI
python train_ai.py --test 9
```
*(Thay số 9 bằng Phase bạn muốn kiểm tra. Lưu ý: Chế độ test không làm thay đổi trọng số của model).*
