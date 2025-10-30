close all;

%% Figure 1: SoC of 15 cells
figure();
hold on;
for i = 1:15
    soc = eval(sprintf('out.SoC_%d', i));
    plot(out.Time.time, soc, 'DisplayName', sprintf('Cell %d', i));
end
hold off;
title('State of Charge (SoC) of 15 Cells');
xlabel('Time [s]');
ylabel('SoC [%]');
legend('show');
grid on;

%% Figure 2: Charge Signal
figure();
plot(out.Time.time, out.ChargeSignal, 'k', 'LineWidth', 1.5);
title('Charge Signal');
xlabel('Time [s]');
ylabel('Signal Level');
grid on;

%% Figure 3: Discharge Signal
figure();
plot(out.Time.time, out.DischargeSignal, 'r', 'LineWidth', 1.5);
title('Discharge Signal');
xlabel('Time [s]');
ylabel('Signal Level');
grid on;

%% Figure 4: Voltage of 15 cells
figure();
hold on;
for i = 1:15
    voltage = eval(sprintf('out.V%d', i));
    plot(out.Time.time, voltage, 'DisplayName', sprintf('Cell %d', i));
end
hold off;
title('Voltage of 15 Cells');
xlabel('Time [s]');
ylabel('Voltage [V]');
legend('show');
grid on;
