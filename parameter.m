%% === Battery Parameters for 15s Configuration ===
% Cố định từng giá trị điện áp và SoC theo bảng lookup

% Điện áp từng cell
V_batt = [ ...
    4.1   % Cell 1
    4.0   % Cell 2
    3.9   % Cell 3
    3.8   % Cell 4
    3.7   % Cell 5
    3.6   % Cell 6
    3.5   % Cell 7
    3.4   % Cell 8
    3.3   % Cell 9
    3.2   % Cell 10
    3.1   % Cell 11
    2.9   % Cell 12
    2.8   % Cell 13
    2.7   % Cell 14
    2.6   % Cell 15
];

% SoC tương ứng (theo bảng tra)
SOC_batt = [ ...
    94.118   % SoC 1
    88.235   % SoC 2
    82.353   % SoC 3
    76.471   % SoC 4
    70.588   % SoC 5
    64.706   % SoC 6
    58.824   % SoC 7
    52.941   % SoC 8
    47.058   % SoC 9
    41.176   % SoC 10
    35.294   % SoC 11
    29.412   % SoC 12
    23.529   % SoC 13
    17.647   % SoC 14
    11.764   % SoC 15
];

BatteryCapacity_Ah = 9;         % Dung lượng 1 cell
BatteryResponseTime = 30;       % Thời gian đáp ứng pin

%% === Passive Components ===
L_value = 1e-3;                 % Inductor 1mH (dùng chung)
C_value = 1e-6;                 % Capacitor 1uF (dùng chung)
R_load  = 20;                    % Resistor tải 1 Ohm

%% === MOSFET Parameters ===
Ron_MOSFET = 0.025;               % FET On-state resistance
Rd_MOSFET  = 1;              % Diode resistance
Vf_MOSFET  = 0.8;                 % Diode forward voltage
Rs_snubber = 1e5;               % Snubber resistor
Cs_snubber = inf;               % Snubber capacitor

%% === Source & Control ===
V_DC_source    = 63;            % Nguồn DC
Duty_constant  = 0.5;           % Duty cycle cố định

%% === PWM Generator (2-Level) ===
PWM_Freq_Hz       = 500;        % PWM frequency
PWM_InitialPhase  = 0;
PWM_MinMax        = [-1, 1];
PWM_SampleTime    = 0;

%% === END ===

