close all;

%% ====== TẠO FIGURE CHUNG ======
figure('Name','Coulomb Counting Comparison','NumberTitle','off');
t = out.Time.time;

% --- Subplot 1: SoC ---
subplot(3,1,1);
plot(t, out.SoC, 'b', 'LineWidth', 1.5); hold on;
plot(t, out.SoC1, 'r--', 'LineWidth', 1.5);
title('State of Charge (SoC) Comparison');
xlabel('Time [s]');
ylabel('SoC [%]');
legend('Battery Block SoC','Coulomb Counting SoC','Location','best');
grid on;

% --- Subplot 2: Current ---
subplot(3,1,2);
plot(t, out.Current, 'm', 'LineWidth', 1.5);
title('Battery Current');
xlabel('Time [s]');
ylabel('Current [A]');
grid on;

% --- Subplot 3: Voltage ---
subplot(3,1,3);
plot(t, out.Voltage, 'k', 'LineWidth', 1.5);
title('Battery Voltage');
xlabel('Time [s]');
ylabel('Voltage [V]');
grid on;

%% ====== TỐI ƯU GIAO DIỆN ======
sgtitle('Comparison of Battery Parameters (SoC, Current, Voltage)');
