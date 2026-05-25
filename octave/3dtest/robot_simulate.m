% robot_simulate.m
% Simulate a 6-joint serial robot arm in GNU Octave

clf;
figure('Name', '6-Arm Robot Simulation', 'Color', [1 1 1]);
hold on;
axis equal;
grid on;
view(45, 20);
camproj('perspective');
xlabel('X');
ylabel('Y');
zlabel('Z');
title('6-Joint Robot Arm Simulation');
rotate3d on;

% Link lengths
L = [1.0 0.8 0.7 0.6 0.5 0.4];

% Base position
base = [0; 0; 0];

% Initial joint angles [deg]
theta = [30; -20; 45; -15; 30; 10];

% Animation settings
numFrames = 120;
for k = 1:numFrames
    cla;

    % Update joint angles with simple sinusoidal motion
    theta = [30; -20; 45; -15; 30; 10] + ...
            [10; 15; 12; 18; 14; 20] .* sin(2*pi*(k/numFrames));

    % Build transform chain
    T = eye(4);
    positions = base;

    for i = 1:6
        % Each joint rotates around Z and then around Y for a wrist-like motion
        Rz = [cosd(theta(i)), -sind(theta(i)), 0, 0;
              sind(theta(i)),  cosd(theta(i)), 0, 0;
              0,               0,              1, 0;
              0,               0,              0, 1];

        Ry = [cosd(theta(i)/2), 0, sind(theta(i)/2), 0;
              0,                1, 0,                0;
             -sind(theta(i)/2), 0, cosd(theta(i)/2), 0;
              0,                0, 0,                1];

        T = T * Rz * Ry;
        T = T * [eye(3), [L(i); 0; 0]; 0 0 0 1];

        positions = [positions, T(1:3,4)];
    end

    % Draw robot links and joints
    plot3(positions(1,:), positions(2,:), positions(3,:), '-o', ...
          'Color', [0 0.4 0.8], 'LineWidth', 3, 'MarkerSize', 6, ...
          'MarkerFaceColor', [0.9 0.3 0.3]);

    % Draw base and target markers
    plot3(base(1), base(2), base(3), 'ks', 'MarkerSize', 10, 'MarkerFaceColor', 'k');

    % Draw a target point for the end effector
    target = [sum(L)*0.5, 0.5, 0.8];
    plot3(target(1), target(2), target(3), 'gd', 'MarkerSize', 10, 'MarkerFaceColor', 'g');

    % Display current end effector position
    ee = positions(:,end);
    text(ee(1)+0.1, ee(2)+0.1, ee(3)+0.1, sprintf('EE (%.2f, %.2f, %.2f)', ee), ...
         'FontSize', 10, 'FontWeight', 'bold');

    xlim([-2, 3.5]);
    ylim([-2, 2.5]);
    zlim([0, 2.5]);

    drawnow;
    pause(0.1);
end

hold off;
