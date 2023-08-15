module crtc(
    input clk,
    input reset,

    input ce_pixel,

    input [1:0] wr,

    input [3:0] address,
    input [15:0] din,
    output [15:0] dout,

    output hsync,
    output hblank,
    output [11:0] hcnt,

    output vsync,
    output vblank,
    output [11:0] vcnt
);

reg [15:0] ctrl[16];

wire [11:0] h_start = ctrl[0][11:0];
wire [11:0] h_end = ctrl[1][11:0];
wire [11:0] hact_end = ctrl[2][11:0];
wire [11:0] hsync_start = ctrl[3][11:0];
wire [11:0] hsync_end = ctrl[4][11:0];

wire [11:0] v_start = ctrl[5][11:0];
wire [11:0] v_end = ctrl[6][11:0];
wire [11:0] vact_end = ctrl[7][11:0];
wire [11:0] vsync_start = ctrl[8][11:0];
wire [11:0] vsync_end = ctrl[9][11:0];

localparam HCNT_REG = 10;
localparam VCNT_REG = 11;

assign hcnt = ctrl[HCNT_REG][11:0];
assign vcnt = ctrl[VCNT_REG][11:0];

assign hblank = hcnt > hact_end || hcnt < h_start;
assign hsync = hcnt > hsync_start && hcnt <= hsync_end;
assign vblank = vcnt > vact_end || vcnt < v_start;
assign vsync = vcnt > vsync_start && vcnt <= vsync_end;
assign dout = ctrl[address];

always_ff @(posedge clk) begin
    if (reset) begin
        ctrl[HCNT_REG] <= 16'd0;
        ctrl[VCNT_REG] <= 16'd0;
    end

    if (wr[0]) ctrl[address][7:0] <= din[7:0];
    if (wr[1]) ctrl[address][15:8] <= din[15:8];

    if (ce_pixel) begin
        ctrl[HCNT_REG] <= hcnt + 12'd1;
        if (hcnt == h_end) begin
            ctrl[HCNT_REG] <= 16'd0;
            ctrl[VCNT_REG] <= vcnt + 12'd1;
            if (vcnt == v_end) begin
                ctrl[VCNT_REG] <= 16'd0;
            end
        end
    end
end

endmodule