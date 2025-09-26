function plot_shaded(varargin)
% PLOT_SHADED(X,Y) - Plots the line with pretty shaded region under the line in the open 
% figure window.
% Inputs:
%     X: vector of domain values.
%     Y: vector or range values.
%
% PLOT_SHADED(X,Y,...)
% Parameter options include:
%     'Alpha': the alpha value of the shaded region, default 0.15.
%     'Color': the shaded region color.
%     'LineWidth': the contour line width, default = 2.0.


[X,Y,color_value,alpha_value,line_width] = parseinputs(varargin{:});


y_min = min(Y);
y_max = max(Y);
V_alpha = alpha_value*((Y-y_min)/y_max);
F = [1 2 3 4];

hold on
% Create patches for the shaded region
for i=1:numel(X)-1
    V = [ X(i) y_min ; X(i) Y(i); X(i+1) Y(i+1); X(i+1) y_min ];
    A = [0,V_alpha(i),V_alpha(i+1),0]';
    patch('Faces',F,'Vertices',V,'FaceColor',color_value,...
        'EdgeColor','none',...
        'FaceVertexAlphaData',A,'FaceAlpha','interp','AlphaDataMapping','none');
end

plot(X,Y,'LineWidth',line_width,'Color',color_value);
hold off

end




function [X,Y,color_value,alpha_value,line_width] = parseinputs(varargin)

% Check the number of input args
minargs = 2;
numopts = 3;
maxargs = minargs + 2*numopts;
narginchk(minargs,maxargs);

ax = gca;

% Set the defaults
alpha_value = 0.15;
color_value = ax.ColorOrder(ax.ColorOrderIndex,:);
line_width = 2.0;


% Get the inputs and check them
X = varargin{1};
validateattributes(X,{'numeric'},{'vector','nonnan','finite'},mfilename,'X',2);
Y = varargin{2};
validateattributes(Y,{'numeric'},{'vector','nonnan','finite','numel',numel(X)},mfilename,'Y',2);
% Ensure that X and Y are column vectors
X = X(:);
Y = Y(:);

if nargin > minargs
    for i=(minargs+1):2:nargin
        PNAME = varargin{i};
        PVALUE = varargin{i+1};
        
        PNAME = validatestring(PNAME,{'Alpha','Color','LineWidth','Prctile'},...
            mfilename,'ParameterName',i);
        
        switch PNAME
            case 'Alpha'
                validateattributes(PVALUE,{'numeric'},{'scalar','nonnan','finite','nonnegative','<=',1.0},mfilename,PNAME,i+1);
                alpha_value = PVALUE;
            case 'Color'
                validateattributes(PVALUE,{'numeric'},{'real','nonnegative','nonempty','vector','numel',3,'<=',1.0},mfilename,PNAME,i+1);
                color_value = PVALUE;
            case 'LineWidth'
                validateattributes(PVALUE,{'numeric'},{'scalar','finite','nonnegative'},...
                    mfilename,PNAME,i+1);
                line_width = PVALUE;
        end
    end
end

end




