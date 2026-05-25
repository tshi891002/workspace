% draw_triangle.m
% Draw a 3D triangle in GNU Octave

% Triangle vertices (x, y, z)
V = [0 0 0;
     1 0 0;
     0.3 0.8 0.6];

% Face indices
F = [1 2 3];

figure('Name', '3D Triangle', 'Color', [1 1 1]);
patch('Vertices', V, 'Faces', F, ...
      'FaceColor', [0.2 0.7 0.9], 'FaceAlpha', 0.6, ...
      'EdgeColor', [0 0 0], 'LineWidth', 1.5);

hold on;
plot3(V(:,1), V(:,2), V(:,3), 'ko', 'MarkerFaceColor', 'k', 'MarkerSize', 8);
text(V(:,1), V(:,2), V(:,3), {'V1','V2','V3'}, 'FontWeight', 'bold', 'FontSize', 12, 'HorizontalAlignment', 'left');

% Draw coordinate axes for reference
quiver3(0, 0, 0, 1.2, 0, 0, 'r', 'LineWidth', 2, 'MaxHeadSize', 0.5);
quiver3(0, 0, 0, 0, 1.2, 0, 'g', 'LineWidth', 2, 'MaxHeadSize', 0.5);
quiver3(0, 0, 0, 0, 0, 1.2, 'b', 'LineWidth', 2, 'MaxHeadSize', 0.5);
text(1.25, 0, 0, 'X', 'FontSize', 12, 'Color', 'r');
text(0, 1.25, 0, 'Y', 'FontSize', 12, 'Color', 'g');
text(0, 0, 1.25, 'Z', 'FontSize', 12, 'Color', 'b');

axis equal;
grid on;
view(45, 30);
camproj('perspective');
xlabel('X');
ylabel('Y');
zlabel('Z');
title('3D Triangle');
rotate3d on;

hold off;
