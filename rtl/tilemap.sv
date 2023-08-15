module tilemap(
    input clk,
    input reset,

    input ce_pixel,

    input [1:0] wr,

    input [15:0] address,
    input [15:0] din,
    output [15:0] dout,

    input [11:0] hcnt,
    input [11:0] vcnt,

    output reg [7:0] color_out
);

wire [15:0] tileref_q;

dualport_ram #(.width(8), .widthad(14)) ram_0
(
    .clock_a(clk),
    .wren_a(wr[0]),
    .address_a(address[13:0]),
    .data_a(din[7:0]),
    .q_a(dout[7:0]),

    .clock_b(clk),
    .wren_b(0),
    .address_b({vcnt[9:3], hcnt[9:3]}),
    .data_b(0),
    .q_b(tileref_q[7:0])
);

dualport_ram #(.width(8), .widthad(14)) ram_1
(
    .clock_a(clk),
    .wren_a(wr[1]),
    .address_a(address[13:0]),
    .data_a(din[15:8]),
    .q_a(dout[15:8]),

    .clock_b(clk),
    .wren_b(0),
    .address_b({vcnt[9:3], hcnt[9:3]}),
    .data_b(0),
    .q_b(tileref_q[15:8])
);

singleport_ram #(.widthad(13), .width(32), .name("GFX"), .init_file("roms/gfx.mif")) gfx_rom(
    .clock(clk),
    .wren(0),
    .address({tileref[9:0], vcnt[2:0]}),
    .data(),
    .q(gfx_data)
);

reg [11:0] vcnt_prev;
reg [15:0] tileref;
wire [31:0] gfx_data;
reg [31:0] shiftout;
reg [3:0] color;
reg [2:0] stage;

always_ff @(posedge clk) begin
    if (ce_pixel) begin
        stage <= stage + 3'd1;

        vcnt_prev <= vcnt;
        if (vcnt_prev != vcnt) begin
            stage <= 3'd0;
        end

        color_out <= { color[3:0], shiftout[31:28]};
        shiftout <= {shiftout[27:0], 4'b0000};

        case(stage)
        3'd0: tileref <= tileref_q;
        3'd7: begin
            color <= tileref[15:12];
            shiftout <= gfx_data;
        end
        endcase
    end
end


endmodule