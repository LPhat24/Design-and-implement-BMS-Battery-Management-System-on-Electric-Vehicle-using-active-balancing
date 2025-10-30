%% ==================== FIREBASE URL ====================
urlMain   = 'https://energy-management-course-default-rtdb.firebaseio.com/Main.json';
urlBattery = 'https://energy-management-course-default-rtdb.firebaseio.com/Battery.json';

disp('Start reading from Firebase...');
tic;

%% ==================== CẤU HÌNH THỜI GIAN ====================
MAX_TIME     = 60 * 60;   % 60 phút = 3600 giây
refresh_dt   = 0.2;       % thời gian cập nhật đồ thị (s)
expand_step  = 10;        % mỗi lần mở rộng trục X thêm 10 giây

%% ==================== KHỞI TẠO DỮ LIỆU ====================
figure(1); clf;
tiledlayout(3,2,'TileSpacing','compact','Padding','compact');

% (1) Tổng điện áp Pack
nexttile;
hTotal = animatedline('Color',[0 0.6 0],'LineWidth',1.5);
title('Total Pack Voltage'); xlabel('Time (s)'); ylabel('Voltage (V)'); grid on;

% (2) ΔV giữa các cell
nexttile;
hDelta = animatedline('Color','m','LineWidth',1.5);
title('Cell Voltage Difference (ΔV)'); xlabel('Time (s)'); ylabel('ΔV (V)'); grid on;

% (3) Dòng điện
nexttile;
hCurrent = animatedline('Color','r','LineWidth',1.5);
title('Battery Current'); xlabel('Time (s)'); ylabel('Current (A)'); grid on;

% (4) Nhiệt độ
nexttile;
hTemp = animatedline('Color','b','LineWidth',1.5);
title('Battery Temperature'); xlabel('Time (s)'); ylabel('Temperature (°C)'); grid on;

% (5) 15 Cell riêng lẻ
nexttile([1 2]);
hold on;
colors = lines(15);
for k = 1:15
    hCell(k) = animatedline('Color',colors(k,:),'LineWidth',1.0);
end
title('Individual Cell Voltages (15 cells)');
xlabel('Time (s)'); ylabel('Voltage (V)');
grid on;
legend(arrayfun(@(k) sprintf('C%02d',k), 1:15, 'UniformOutput', false), ...
       'Location','eastoutside','FontSize',7);
drawnow;

%% ==================== VÒNG LẶP CHÍNH ====================
xMaxDisplay = expand_step;  % giá trị ban đầu của trục X

while true
    try
        % ==== Đọc dữ liệu từ Firebase ====
        dataMain   = webread(urlMain);
        dataBattery = webread(urlBattery);
        t = toc; % thời gian (s)

        % Dừng sau 60 phút
        if t >= MAX_TIME
            disp('⏹ Đã đạt 60 phút, dừng mô phỏng.');
            break;
        end

        % ==== Đọc giá trị ====
        % Cell voltages
        cellVals = NaN(1,15);
        for k = 1:15
            key = sprintf('Cell%02d', k);
            if isfield(dataBattery, key)
                cellVals(k) = str2double(string(dataBattery.(key)));
            end
        end

        % Các giá trị tổng
        i = NaN; temp = NaN; dv = NaN; vtot = NaN;
        if isfield(dataMain,'Battery_Current_A')
            i = str2double(string(dataMain.Battery_Current_A));
        end
        if isfield(dataMain,'Temperature_C')
            temp = str2double(string(dataMain.Temperature_C));
        end
        if isfield(dataMain,'Cell_Delta_mV')
            dv = str2double(string(dataMain.Cell_Delta_mV)) / 1000.0; % mV → V
        elseif isfield(dataMain,'Cell_DeltaV_V')
            dv = str2double(string(dataMain.Cell_DeltaV_V));
        end
        if isfield(dataMain,'Total_Voltage_V')
            vtot = str2double(string(dataMain.Total_Voltage_V));
        end

        % ==== Cập nhật dữ liệu vào đồ thị ====
        addpoints(hTotal,  t, vtot);
        addpoints(hDelta,  t, dv);
        addpoints(hCurrent,t, i);
        addpoints(hTemp,   t, temp);
        for k = 1:15
            addpoints(hCell(k), t, cellVals(k));
        end

        % ==== Mở rộng trục X dần theo thời gian (từ 0 → t) ====
        if t > xMaxDisplay
            xMaxDisplay = xMaxDisplay + expand_step;
        end

        allAxes = findall(gcf,'Type','axes');
        for ax = allAxes'
            ax.XLim = [0, xMaxDisplay];  % luôn bắt đầu từ 0
        end

        drawnow limitrate nocallbacks;

    catch ME
        warning('⚠️ Error reading Firebase: %s', ME.message);
        pause(1.0); % chờ 1s rồi thử lại
    end

    pause(refresh_dt);
end

disp('✅ Simulation completed successfully!');
