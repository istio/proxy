function [bin_centers,hist_values,bin_edges] = plot_histogram_shaded(varargin)
% [C,V,E] = PLOT_HISTOGRAM_SHADED(X) - Plots the histogram as a shaded line plot instead of bar plot. 
% Outputs:
%     C: the histogram bin centers.
%     V: the histogram values at each bin.
%     E: the histogram bin edges values.
%
% [C,V,E] = PLOT_HISTOGRAM_SHADED(X,_)
% Optional values:
%     'alpha': alpha value of the shaded region, default 0.2.
%     'bins': number of bins, default [].
%     'color': color of the plot, default [0 0 1].
%     'edges': the bins edges to use, default [].
%     'normalization': the normalization type to use (see histogram doc),
%              default 'probability'
%

[X,n_bins,bin_edges,color_value,alpha_value,norm_type] = parseinputs(varargin{:});



if isempty(n_bins) && isempty(bin_edges)
    [hist_values,bin_edges] = histcounts(X,'Normalization',norm_type);
else
    bin_param = n_bins;
    if ~isempty(bin_edges)
        bin_param = bin_edges;
    end
    [hist_values,bin_edges] = histcounts(X,bin_param,'Normalization',norm_type);    
end


x_cat = cat(1,bin_edges,circshift(bin_edges,-1));
x_mean = mean(x_cat,1);
bin_centers = x_mean(1:end-1);
fprintf(1,'Plotting histogram with %d bins\n',numel(bin_centers));

% Use a shaded plot
plot_shaded(bin_centers,hist_values,'Alpha',alpha_value,'Color',color_value,'LineWidth',1.5);


end


function [X,n_bins,bin_edges,color_value,alpha_value,norm_type] = parseinputs(varargin)

% Get the number of input args
minargs = 1;
maxargs = minargs+5*2;
narginchk(minargs,maxargs);

ax = gca;

% Set the defaults
n_bins = [];
bin_edges = [];
color_value = ax.ColorOrder(ax.ColorOrderIndex,:);
alpha_value = 0.2;
norm_type = 'probability';

% Get the inputs and check them
X = varargin{1};
validateattributes(X,{'numeric'},{'nonnan','finite'},mfilename,'X',1);

if nargin > 1
    for i=2:2:nargin
        PNAME = varargin{i};
        PVALUE = varargin{i+1};
        
        PNAME = validatestring(PNAME,...
            {'alpha','bins','color','edges','normalization'},...
            mfilename,'ParameterName',i);
        
        switch PNAME
            case 'alpha'
                validateattributes(PVALUE,{'numeric'},{'real','nonnegative','finite','scalar'},...
                    mfilename,PNAME,i+1);
                alpha_value = PVALUE;
            case 'bins'
                if ~isempty(PVALUE)
                    validateattributes(PVALUE,{'numeric'},{'integer','positive','finite','scalar'},...
                        mfilename,PNAME,i+1);
                    n_bins = PVALUE;
                end
            case 'color'
                validateattributes(PVALUE,{'numeric'},{'real','nonnegative','nonempty','vector','numel',3,'<=',1.0},mfilename,PNAME,i+1);
                color_value = PVALUE;
            case 'edges'
                validateattributes(PVALUE,{'numeric'},{'real','nonempty','vector','nonnan'},mfilename,PNAME,i+1);
                bin_edges = PVALUE;
            case 'normalization'
                validateattributes(PVALUE,{'char'},{'nonempty'},mfilename,PNAME,i+1);
                norm_type = PVALUE;
        end
    end
end

end

