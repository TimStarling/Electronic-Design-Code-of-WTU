n = 1024;
t = 0:n-1;
sine_q = round((sin(2*pi*t/n) + 1) * 2047.5); % 0–4095 scaled 0~4095量化

% 打印为可复制的1024个数据（每行8个，带逗号）
% Print copyable 1024 data (8 per line, comma-separated)
for i = 1:n
    fprintf('%d', sine_q(i));
    if i < n
        fprintf(', ');
    end
    if mod(i,8) == 0
        fprintf('\n');
    end
end
