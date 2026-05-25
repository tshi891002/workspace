% draw_car.m
% Draw a simple 3D car in GNU Octave

clf;
figure('Name', '3D Car', 'Color', [1 1 1]);
hold on;

% Car body vertices
body = [0 0 0;
        3 0 0;
        3 1.5 0;
        0 1.5 0;
        0 0 0.8;
        3 0 0.8;
        3 1.5 0.8;
        0 1.5 0.8];

% Body faces
faces = [1 2 3 4;    % bottom
         5 6 7 8;    % roof
         1 2 6 5;    % front
         2 3 7 6;    % right
         3 4 8 7;    % rear
         4 1 5 8];   % left

patch('Vertices', body, 'Faces', faces, ...
      'FaceColor', [0.9 0.1 0.1], 'EdgeColor', 'k', 'FaceAlpha', 0.9);

% Windshield and windows
window = [0.5 0 0.8;
          2.2 0 0.8;
          2.2 0 0.4;
          0.5 0 0.4];
patch('Vertices', window, 'Faces', [1 2 3 4], ...
      'FaceColor', [0.2 0.5 0.8], 'FaceAlpha', 0.4, 'EdgeColor', 'none');

window_side = [0.5 1.5 0.8;
               2.2 1.5 0.8;
               2.2 1.5 0.4;
               0.5 1.5 0.4];
patch('Vertices', window_side, 'Faces', [1 2 3 4], ...
      'FaceColor', [0.2 0.5 0.8], 'FaceAlpha', 0.4, 'EdgeColor', 'none');

% Wheels
[r, z] = cylinder(0.25, 30);
wheel_z = z * 0.3 - 0.15;
for offset = [0.4 1.1 1.9 2.6]
    % front and rear wheels
    X = r + offset;
    Y = ones(size(X)) * 0.2;
    patch(surf2patch(X, Y, wheel_z), 'FaceColor', [0.1 0.1 0.1], 'EdgeColor', 'none');
    Y = ones(size(X)) * 1.3;
    patch(surf2patch(X, Y, wheel_z), 'FaceColor', [0.1 0.1 0.1], 'EdgeColor', 'none');
end

% Details: headlights and grille
patch('Vertices', [3 0.2 0.35; 3.1 0.2 0.35; 3.1 0.2 0.55; 3 0.2 0.55], 'Faces', [1 2 3 4], ...
      'FaceColor', [1 1 0.4], 'FaceAlpha', 0.8, 'EdgeColor', 'none');
patch('Vertices', [3 1.3 0.35; 3.1 1.3 0.35; 3.1 1.3 0.55; 3 1.3 0.55], 'Faces', [1 2 3 4], ...
      'FaceColor', [1 1 0.4], 'FaceAlpha', 0.8, 'EdgeColor', 'none');

% Axes and view
axis equal;
grid on;
view(40, 25);
camproj('perspective');
xlabel('X');
ylabel('Y');
zlabel('Z');
title('3D Car');
rotate3d on;

hold off;
