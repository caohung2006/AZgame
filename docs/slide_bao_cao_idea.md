# ĐỀ CƯƠNG SLIDE BÁO CÁO DỰ ÁN: HUẤN LUYỆN AI XE TĂNG ĐỐI KHÁNG (AZ TANK GAME)
### Môn học: IT3160 - Giới thiệu Trí tuệ nhân tạo / Dự án CNTT

---

## CẤU TRÚC TỔNG THỂ BÀI TRÌNH BÀY (14 SLIDES)
* **Phần 1: Giới thiệu & Đặt vấn đề** (Slides 1 - 3)
* **Phần 2: Mô hình hóa Học tăng cường (RL)** (Slides 4 - 6)
* **Phần 3: Kiến trúc Hệ thống & Giải pháp huấn luyện** (Slides 7 - 11)
* **Phần 4: Kết quả thực nghiệm & Kết luận** (Slides 12 - 14)

---

## PHẦN 1: GIỚI THIỆU & ĐẶT VẤN ĐỀ

### SLIDE 1: TRANG TIÊU ĐỀ
* **Bố cục hiển thị**:
  * Trái: Tên đề tài nổi bật, thông tin sinh viên & GVHD.
  * Phải: Hình ảnh/Video động gameplay thực tế (Xe tăng chiến đấu trong mê cung, đạn nảy phát sáng).
* **Nội dung trên Slide**:
  * **ĐỀ TÀI**: HUẤN LUYỆN TÁC NHÂN AI XE TĂNG ĐỐI KHÁNG TRONG MÔI TRƯỜNG PHỨC TẠP
  * **Môn học**: Dự án CNTT / Giới thiệu Trí tuệ Nhân tạo - IT3160
  * **Sinh viên thực hiện**: [Tên sinh viên] - [MSSV]
  * **Giảng viên hướng dẫn**: [Tên giảng viên]
* **Gợi ý lời thoại thuyết trình**:
  > *"Kính chào Thầy/Cô và các bạn. Hôm nay em xin phép trình bày dự án học phần IT3160 với đề tài: Huấn luyện tác nhân AI xe tăng đối kháng tự hành trong trò chơi AZ Tank Game. Đây là một dự án kết hợp giữa lập trình vật lý game thời gian thực và công nghệ Học tăng cường."*

---

### SLIDE 2: ĐẶT VẤN ĐỀ & THÁCH THỨC
* **Bố cục hiển thị**: Chia 2 cột so sánh trực quan (Hình ảnh minh họa cho mỗi bên).
  * Cột 1: Môi trường AZ Tank Game (Động, khó đoán định).
  * Cột 2: Sự hạn chế của Bot lập trình luật (Rule-Based).
* **Nội dung trên Slide**:
  * **Đặc trưng trò chơi**:
    * Mê cung sinh ngẫu nhiên $\to$ Không thể ghi nhớ bản đồ cố định.
    * Đạn nảy tường vật lý $\to$ Quỹ đạo bay đa chiều, nguy cơ tự sát cao.
    * Cổng dịch chuyển (Portal) & Hộp vật phẩm $\to$ Thay đổi tức thời cục diện.
  * **Hạn chế của Bot lập trình luật truyền thống**:
    * Cấu trúc `if-else` khổng lồ $\to$ Dễ lỗi, khó bảo trì.
    * Thiếu tính linh hoạt $\to$ Dễ bị bắt bài, không thích nghi được địa hình mới.
* **Gợi ý lời thoại thuyết trình**:
  > *"Tại sao chúng ta không dùng Bot lập trình luật thông thường? Game xe tăng của chúng ta có mê cung tạo ngẫu nhiên, cổng dịch chuyển và đặc biệt là đạn nảy tường. Nếu viết code luật thông thường bằng `if-else`, chương trình sẽ cực kỳ cồng kềnh, dễ bị lỗi và không thể bao quát hết mọi tình huống địa hình phát sinh. Do đó, chúng ta cần một giải pháp AI tự học để thích ứng."*

---

### SLIDE 3: MỤC TIÊU DỰ ÁN
* **Bố cục hiển thị**: 3 thẻ (Cards) xếp ngang thể hiện 3 mục tiêu cốt lõi. Dùng icon đại diện cho Nghiên cứu, Kỹ thuật, Thực nghiệm.
* **Nội dung trên Slide**:
  * **Nghiên cứu lý thuyết**:
    * Áp dụng thuật toán PPO và kỹ thuật GAE.
    * Giải quyết bài toán phần thưởng thưa thớt (Sparse Rewards) bằng Tạo hình phần thưởng (Reward Shaping).
  * **Giải pháp kỹ thuật**:
    * Tích hợp mượt mà **C++/Box2D** (Game Engine) và **Python** (Học máy) qua **Pybind11**.
    * Tối ưu hóa hiệu năng huấn luyện đa luồng trên CPU.
  * **Đánh giá thực nghiệm**:
    * Xây dựng tiến trình huấn luyện lũy tiến và cơ chế tự đối đầu (Self-Play).
    * AI phải đánh bại mọi cấp độ Bot luật từ dễ đến khó.
* **Gợi ý lời thoại thuyết trình**:
  > *"Dự án đặt ra 3 mục tiêu chính: Thứ nhất, nghiên cứu sâu lý thuyết PPO và tìm cách thiết kế hàm phần thưởng hợp lý trong môi trường mê cung. Thứ hai, tích hợp thành công lõi game C++ với thư viện AI Python để huấn luyện đạt tốc độ tối đa. Và cuối cùng là kiểm chứng khả năng của AI qua mô hình tự đối đầu và chiến thắng các Bot luật."*

---

## PHẦN 2: MÔ HÌNH HÓA HỌC TĂNG CƯỜNG (RL)

### SLIDE 4: KHÔNG GIAN HÀNH ĐỘNG (ACTION SPACE)
* **Bố cục hiển thị**: Sơ đồ khối minh họa 3 nhánh đầu ra của mạng nơ-ron hoạt động song song. Dùng hình ảnh điều khiển tương đương bàn phím.
* **Nội dung trên Slide**:
  * **Kiến trúc**: MultiDiscrete Action Space `[3, 3, 2]`
  * **3 nhóm quyết định đồng thời**:
    * **Nhánh 1 - Di chuyển**: `[0: Đứng yên, 1: Tiến, 2: Lùi]`
    * **Nhánh 2 - Xoay thân**: `[0: Không xoay, 1: Xoay trái, 2: Xoay phải]`
    * **Nhánh 3 - Khai hỏa**: `[0: Giữ súng, 1: Bắn]`
  * **Ưu điểm**:
    * Phản ánh chính xác cơ chế bấm phím của con người.
    * Giảm độ phức tạp của không gian tìm kiếm so với hành động liên tục, giúp AI hội tụ nhanh hơn.
* **Gợi ý lời thoại thuyết trình**:
  > *"Để điều khiển xe tăng, mạng nơ-ron sẽ đưa ra đồng thời 3 quyết định độc lập ở mỗi bước thời gian: tiến lùi, xoay thân và bắn hay không. Thiết kế dạng rời rạc đa nhánh này giúp mô phỏng chính xác cách con người chơi game, đồng thời giúp AI học nhanh hơn nhiều so với việc điều khiển dạng số thực liên tục."*

---

### SLIDE 5: KHÔNG GIAN TRẠNG THÁI (OBSERVATION SPACE)
* **Bố cục hiển thị**: Trái: Sơ đồ xe tăng AI với các tia quét cảm biến (Vẽ trực quan 8 tia quét tường, 2 tia quét đạn, A* chỉ đường). Phải: Danh sách các thành phần của vector 52 chiều.
* **Nội dung trên Slide**:
  * **Vector trạng thái chuẩn hóa (52 chiều, khoảng $[-1.0, 1.0]$)**:
    * **Bản thân (Self)**: Tốc độ tuyến tính, tốc độ góc, hướng xoay (5 chiều).
    * **Đối thủ (Enemy)**: Khoảng cách, hướng tương đối, tầm nhìn thẳng (10 chiều).
    * **Đạn nguy hiểm (Bullet Radar)**: Vị trí, thời gian va chạm ước tính của 2 viên đạn gần nhất (8 chiều).
    * **Tường chắn (Wall Radar)**: Khoảng cách theo 8 hướng quét (8 chiều).
    * **Dẫn đường (A\* Waypoint)**: Vị trí điểm waypoint tiếp theo hướng tới mục tiêu (3 chiều).
    * **Trạng thái & Vũ khí (Status)**: Lượng đạn, khiên hồi, loại súng đang cầm (18 chiều).
* **Gợi ý lời thoại thuyết trình**:
  > *"Làm sao AI 'nhìn' thấy môi trường? Chúng ta không đưa ảnh màn hình vào huấn luyện vì rất nặng. Thay vào đó, AI nhận một vector 52 chiều chứa các thông tin cảm biến cục bộ xung quanh nó. Bao gồm trạng thái bản thân, thông tin kẻ địch, 8 hướng quét tường tĩnh, radar cảnh báo đạn bay tới và thông tin dẫn đường từ giải thuật A*."*

---

### SLIDE 6: CHIẾN LƯỢC TẠO HÌNH PHẦN THƯỞNG (REWARD SHAPING)
* **Bố cục hiển thị**: Một sơ đồ hình phễu thể hiện sự phân cấp phần thưởng từ Phần thưởng thưa thớt (Sparse) ở đỉnh xuống Phần thưởng dẫn dắt (Dense) ở đáy.
* **Nội dung trên Slide**:
  * **Phần thưởng cốt lõi (Sparse)**: Thắng trận ($+5.0$), Thua trận ($-5.0$), Tự sát do đạn nảy trúng mình ($-10.0$).
  * **Phần thưởng dẫn dắt (Dense)**:
    * *Di chuyển*: Đi theo waypoint A*, áp sát kẻ địch.
    * *Chiến đấu*: Thưởng bắn khi thấy địch, phạt bắn mù (lãng phí đạn).
    * *Né tránh*: Phạt khi ở gần đạn địch, thưởng sống sót.
    * *An toàn*: Phạt đâm tường, phạt kẹt góc, phạt đứng yên thụ động (Camping).
  * **Cơ chế giảm dần Shaping Factor ($SF$)**:
    * $SF$ giảm từ $1.0 \to 0.0$ theo tiến trình huấn luyện.
    * **Mục đích**: Ban đầu dẫn dắt AI tập đi/tập bắn, về sau loại bỏ dần để AI chỉ tập trung vào chiến thắng cuối cùng.
* **Gợi ý lời thoại thuyết trình**:
  > *"Để dẫn dắt AI trong mê cung, chúng tôi xây dựng hàm phần thưởng phân cấp. Ngoài điểm thắng/thua cuối trận, AI được thưởng/phạt liên tục ở mỗi bước: thưởng đi đúng đường A*, phạt khi đâm đầu vào tường hay đứng yên một chỗ, phạt khi đạn địch bay gần để học cách né. Để tránh AI chỉ chăm chăm nhặt điểm dẫn dắt mà quên đi mục tiêu thắng trận, hệ số dẫn dắt SF sẽ tự động giảm dần về 0 ở cuối quá trình huấn luyện."*

---

## PHẦN 3: KIẾN TRÚC HỆ THỐNG & GIẢI PHÁP HUẤN LUYỆN

### SLIDE 7: KIẾN TRÚC CÔNG NGHỆ PPO & GAE
* **Bố cục hiển thị**: Sơ đồ cấu trúc mạng Actor-Critic đơn giản (Khối Input $\to$ Tầng ẩn $\to$ Nhánh Actor & Nhánh Critic). Có nhãn chú thích cơ chế "Clip" của PPO.
* **Nội dung trên Slide**:
  * **Kiến trúc Actor - Critic**:
    * **Actor (Chính sách)**: Mạng MLP (2 tầng ẩn, kích thước 64/128) $\to$ Quyết định hành động.
    * **Critic (Đánh giá)**: Ước lượng giá trị của trạng thái $\to$ Giúp Actor tự sửa sai.
  * **Thuật toán PPO (Proximal Policy Optimization)**:
    * Giới hạn tỷ lệ thay đổi chính sách trong khoảng an toàn $[1-\epsilon, 1+\epsilon]$.
    * Ngăn chặn việc học quá đà làm hỏng chính sách tốt đã có.
  * **Ước lượng lợi thế GAE**:
    * Tính toán giá trị lợi thế $\hat{A}_t$ hiệu quả, tối ưu hóa quá trình cập nhật trọng số mạng.
* **Gợi ý lời thoại thuyết trình**:
  > *"Về thuật toán, chúng tôi sử dụng PPO với mạng nơ-ron liên kết đầy đủ dạng MLP. Thuật toán hoạt động theo cơ chế Actor-Critic: Actor đưa ra hành động, Critic đánh giá xem hành động đó tốt hay xấu. PPO có ưu điểm vượt trội nhờ cơ chế Clip giới hạn tốc độ thay đổi chính sách, giúp quá trình học không bị sụp đổ hiệu năng đột ngột."*

---

### SLIDE 8: LUỒNG HOẠT ĐỘNG HỆ THỐNG (C++ & PYTHON)
* **Bố cục hiển thị**: Sơ đồ chu kỳ khép kín (Sequence Diagram hoặc Flowchart ngang) thể hiện đường đi của dữ liệu.
  * Game Engine C++ $\xrightarrow{\text{Pybind11}}$ Wrapper Python $\xrightarrow{\text{NumPy}}$ Học máy PPO $\xrightarrow{\text{Hành động}}$ Game Engine.
* **Nội dung trên Slide**:
  * **Game Engine (C++)**: Tính toán vật lý Box2D, va chạm, tạo mê cung (Tốc độ tối đa).
  * **Pybind11**: Cầu nối chuyển đổi dữ liệu C++ sang Python trực tiếp trong bộ nhớ (Không ghi đĩa, không độ trễ).
  * **Gymnasium Wrapper (Python)**: Chuẩn hóa dữ liệu đầu vào và phân phối phần thưởng.
  * **Stable-Baselines3**: Huấn luyện mạng nơ-ron trên CPU (Đồng bộ tốc độ vật lý game, tránh nghẽn RAM-VRAM).
* **Gợi ý lời thoại thuyết trình**:
  > *"Hệ thống hoạt động theo một vòng lặp khép kín cực kỳ nhanh. Lõi game viết bằng C++ và thư viện vật lý Box2D để chạy mượt mà ở 60 FPS. Thông qua Pybind11, trạng thái vật lý của game được chuyển đổi tức thời sang dạng mảng NumPy trong Python mà không gặp độ trễ. Mạng PPO nhận trạng thái này, đưa ra hành động và gửi ngược lại cho game engine cập nhật bước tiếp theo."*

---

### SLIDE 9: LỘ TRÌNH HUẤN LUYỆN LŨY TIẾN (CURRICULUM LEARNING)
* **Bố cục hiển thị**: Trình bày dưới dạng Dòng thời gian (Timeline) từ trái qua phải, gồm 4 cụm chính (Tương ứng 11 giai đoạn).
* **Nội dung trên Slide**:
  * **Lộ trình 11 giai đoạn nâng dần độ khó**:
    * **Giai đoạn 1 - 2**: Đấu với bia đứng yên & bia di động trên bãi trống $\to$ *Học cách di chuyển và bắn thẳng.*
    * **Giai đoạn 3 - 4**: Đối phương biết bắn trả trên bãi trống $\to$ *Học cách kết hợp di chuyển, bắn và né tránh.*
    * **Giai đoạn 5 - 7**: Đưa vào mê cung vật lý, đối phương bắn nảy tường 1-2 lần $\to$ *Học điều hướng mê cung và tận dụng đạn nảy.*
    * **Giai đoạn 8 - 11**: Bản đồ đầy đủ Portal, Khiên, Hộp vật phẩm và đối đầu Boss Sniper $\to$ *Rèn luyện chiến thuật nâng cao.*
* **Gợi ý lời thoại thuyết trình**:
  > *"Để AI có thể tự học trong một môi trường phức tạp như vậy, chúng tôi áp dụng phương pháp huấn luyện lũy tiến gồm 11 giai đoạn. AI bắt đầu học bằng cách bắn bia đứng yên trên bãi trống, sau đó bia bắt đầu chạy, rồi bia biết bắn trả. Tiếp theo, chúng tôi thả AI vào mê cung, nâng dần cấp độ bắn nảy của bot đối thủ, và cuối cùng mới tích hợp portal, vật phẩm và khiên chắn để AI đạt kỹ năng toàn diện."*

---

### SLIDE 10: CƠ CHẾ TỰ ĐỐI ĐẦU (SELF-PLAY) TRÁNH OVERFITTING
* **Bố cục hiển thị**: Sơ đồ vòng tròn tương tác. AI hiện tại $\leftrightarrow$ Đối thủ (được chọn ngẫu nhiên từ bể lưu trữ các phiên bản checkpoint cũ và bot luật).
* **Nội dung trên Slide**:
  * **Nguy cơ**: AI bị "học tủ" (chỉ thắng được một thuật toán bot cố định).
  * **Giải pháp: Bể đối thủ (Opponent Pool)**:
    * Lưu trữ các phiên bản lịch sử của chính AI trong quá trình huấn luyện.
    * Lưu trữ các Bot luật thông minh viết bằng C++.
  * **Luồng hoạt động**:
    * Khi bắt đầu ván mới $\to$ Bốc ngẫu nhiên 1 đối thủ trong bể.
    * Sau mỗi $100,000$ bước huấn luyện $\to$ Lưu mô hình hiện tại nạp ngược lại vào bể làm đối thủ tiếp theo.
* **Gợi ý lời thoại thuyết trình**:
  > *"Một lỗi rất hay gặp trong AI game đối kháng là 'học tủ'. AI đấu với một đối thủ cố định lâu ngày sẽ tìm ra khe hở của đối thủ đó để thắng, nhưng khi gặp đối thủ khác lại thua. Để khắc phục, chúng tôi dùng cơ chế Self-Play. AI sẽ đấu với chính các phiên bản cũ của mình được lưu trong Opponent Pool. Đối thủ sẽ tự động nâng cấp đồng hành cùng AI, giúp chính sách học được có tính tổng quát cao."*

---

### SLIDE 11: ĐỐI THỦ HUÂN LUYỆN: C++ BOT ĐA LUỒNG CỰC MẠNH
* **Bố cục hiển thị**: Chia làm 2 cột.
  * Cột 1: Kiến trúc đa luồng của Bot (Luồng di chuyển & Luồng ngắm bắn song song).
  * Cột 2: Sơ đồ thuật toán bắn nảy tường **FindBounce**.
* **Nội dung trên Slide**:
  * **Kiến trúc đa luồng**:
    * Tách riêng luồng lái xe (nhận diện chướng ngại vật) và luồng ngắm bắn nảy tường để không làm giảm khung hình game.
  * **Thuật toán bắn nảy tường (FindBounce)**:
    * Quét 180 tia xung quanh nòng súng, giả lập phản xạ nảy tối đa 4 lần.
    * **Kiểm tra tự sát (Self-hit check)**: Mô phỏng trước quỹ đạo đạn, loại bỏ ngay các góc bắn có nguy cơ nảy ngược lại trúng thân xe ở hiện tại hoặc vị trí di chuyển dự kiến.
* **Gợi ý lời thoại thuyết trình**:
  > *"Để làm 'người thầy' huấn luyện cho AI, chúng tôi đã viết một Bot luật bằng C++ cực kỳ mạnh mẽ. Bot này chạy đa luồng để tính toán song song việc di chuyển và ngắm bắn. Đặc biệt, thuật toán FindBounce của Bot sẽ giả lập 180 hướng bắn với tối đa 4 lần nảy đạn. Bot cũng tự động tính toán để loại bỏ các góc bắn có thể nảy ngược lại gây tự sát, tạo ra áp lực rất lớn ép AI phải học cách né tránh."*

---

## PHẦN 4: KẾT QUẢ THỰC NGHIỆM & KẾT LUẬN

### SLIDE 12: KẾT QUẢ THỰC NGHIỆM & KỸ NĂNG CỦA AI
* **Bố cục hiển thị**: 4 ô lưới (Grid) hiển thị 4 kỹ năng chính của AI. Kèm theo hình ảnh chụp khoảnh khắc AI thực hiện kỹ năng đó.
* **Nội dung trên Slide**:
  * **Kỹ năng tự học được của tác nhân AI (PPO Agent)**:
    * **1. Ôm cua & Thoát kẹt**: Di chuyển mượt mà qua các góc hẹp của mê cung, tự lùi và quay đầu khi bị kẹt góc tường.
    * **2. Né đạn chủ động**: Nhận diện đạn địch bay tới gần, chủ động đánh lái vuông góc để né tránh.
    * **3. Cướp tài nguyên**: Di chuyển ưu tiên nhặt hộp vật phẩm, tự động kích hoạt khiên chắn đúng thời điểm đạn sắp chạm vào người.
    * **4. Bắn nảy góc khuất**: Học được cách bắn đạn nảy qua các vách tường để hạ gục đối thủ trốn sau chướng ngại vật.
* **Gợi ý lời thoại thuyết trình**:
  > *"Kết quả thực nghiệm cho thấy AI đã tự học được những kỹ năng chiến thuật rất thông minh mà không cần chúng ta lập trình sẵn. AI biết đi qua các ngã ba mê cung không bị kẹt, biết né đạn chủ động khi thấy nguy hiểm, biết nhặt vật phẩm để tăng sức mạnh và biết bắn nảy tường để tiêu diệt đối thủ trốn sau góc khuất."*

---

### SLIDE 13: ĐÁNH GIÁ HIỆU SUẤT ĐÀO TẠO
* **Bố cục hiển thị**: Trái: Biểu đồ hội tụ phần thưởng tích lũy (Cumulative Reward) tăng dần qua các phase. Phải: Các thông số hiệu năng phần cứng và phần mềm dạng số lớn nổi bật.
* **Nội dung trên Slide**:
  * **Hiệu năng hệ thống**:
    * Tốc độ lấy mẫu: **1,200 - 1,800 steps/giây** (Huấn luyện hoàn toàn trên CPU Intel Core i7 / Ryzen 7).
    * Tổng số bước huấn luyện: **26.3 triệu steps**.
    * Tổng thời gian chạy: **~4.5 tiếng** (Nhanh gấp 30 lần thời gian thực tế chạy game).
  * **Độ hội tụ**:
    * Hàm phần thưởng tích lũy tăng đều đặn qua các phase, chứng tỏ AI học thành công các bài học mới.
    * Sai số hàm giá trị (Value Loss) tiệm cận về mức thấp và ổn định.
* **Gợi ý lời thoại thuyết trình**:
  > *"Nhờ việc tối ưu hóa chạy hoàn toàn trên CPU và loại bỏ độ trễ truyền dữ liệu đồ họa, tốc độ huấn luyện đạt mức rất cao, từ 1,200 đến 1,800 bước vật lý mỗi giây. Nhờ đó, chúng tôi chỉ mất khoảng 4 tiếng rưỡi để huấn luyện xong toàn bộ 26 triệu bước của 11 giai đoạn. Biểu đồ Tensorboard cho thấy phần thưởng tích lũy tăng trưởng ổn định qua từng giai đoạn và mô hình đã hội tụ thành công."*

---

### SLIDE 14: KẾT LUẬN & HƯỚNG PHÁT TRIỂN
* **Bố cục hiển thị**: Chia làm 2 cột rõ ràng: Kết luận (Trái) và Hướng phát triển (Phải).
* **Nội dung trên Slide**:
  * **Kết luận**:
    * Ứng dụng thành công PPO và GAE huấn luyện AI điều khiển xe tăng đối kháng linh hoạt.
    * Curriculum Learning và Self-Play là chìa khóa giải quyết bài toán phần thưởng thưa thớt và overfitting.
    * Sự tích hợp C++/Python mang lại hiệu năng huấn luyện tối ưu với chi phí phần cứng thấp.
  * **Hướng phát triển**:
    * Tích hợp mạng bộ nhớ **LSTM/GRU** giúp AI nhớ quỹ đạo đạn nảy ẩn khuất.
    * Mở rộng thành bài toán Multi-Agent (Hỗn chiến đồng đội 2v2 hoặc Battle Royale).
    * Thử nghiệm trên môi trường 3D.
* **Gợi ý lời thoại thuyết trình**:
  > *"Tóm lại, dự án đã chứng minh tính hiệu quả của PPO và học lũy tiến trong môi trường game vật lý phức tạp. Trong tương lai, chúng tôi hướng tới việc tích hợp mạng bộ nhớ tuần hoàn LSTM giúp AI ghi nhớ hướng đạn nảy tốt hơn nữa, đồng thời mở rộng mô hình cho đấu đội nhiều xe tăng. Em xin chân thành cảm ơn Thầy/Cô và các bạn đã lắng nghe. Em xin sẵn sàng nhận các câu hỏi góp ý."*
