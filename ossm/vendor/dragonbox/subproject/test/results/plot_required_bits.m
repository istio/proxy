function [] = plot_required_bits(filename)

table = readtable(filename);
fig = figure('Color','w');
hold on
mul_fig_handle = plot(table{:,1}, table{:,2});
int_fig_handle = plot(table{:,1}, table{:,3});
legend([mul_fig_handle int_fig_handle], ...
    ["For multiplication", "For integer check"]);
axis([min(table{:,1}) max(table{:,1}) min(min(table{:,2:3})) max(max(table{:,2:3}))]);
xlabel('$e$', 'Interpreter', 'latex');
ylabel('Upper bound on required number of bits');
h = gca;
h.YTick = 0:8:max(max(table{:,2:3}));
h.YGrid = 'on';
hold off

end