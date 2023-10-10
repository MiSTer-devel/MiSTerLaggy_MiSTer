module blitter(
    input clk,
    input reset,

    output reg blit_active,
    output reg blit_wr,
    output reg [15:0] blit_addr,
    output reg [15:0] blit_dout,

    input [1:0] wr,

    input [15:0] address,
    input [15:0] din,
    output reg [15:0] dout
);

reg [15:0] blit_value;
reg [15:0] blit_span;
reg [15:0] blit_skip;
reg [15:0] blit_repeat;
reg [15:0] blit_start;

function [15:0] word_assign(input [15:0] cur, input [15:0] data, input [1:0] wr);
    begin
        word_assign = { wr[1] ? data[15:8] : cur[15:8], wr[0] ? data[7:0] : cur[7:0] };
    end
endfunction


reg [15:0] blit_next_addr, blit_span_count, blit_repeat_count;

always_ff @(posedge clk) begin
    blit_wr <= 0;
    if (reset) begin
        blit_active <= 0;
    end else if (blit_active) begin
        if (blit_repeat_count > blit_repeat) begin
            blit_active <= 0;
        end else begin
            blit_wr <= 1;
            blit_addr <= blit_next_addr;
            blit_dout <= blit_value;
            blit_span_count <= blit_span_count + 16'd1;
            if (blit_span_count == blit_span) begin
                blit_span_count <= 0;
                blit_repeat_count <= blit_repeat_count + 16'd1;
                blit_next_addr <= blit_next_addr + blit_skip;
            end else begin
                blit_next_addr <= blit_next_addr + 16'd1;
            end
        end
    end else begin
        case(address[3:0])
        0: begin
            blit_value <= word_assign(blit_value, din, wr);
            dout <= blit_value;
        end
        1: begin
            blit_span <= word_assign(blit_span, din, wr);
            dout <= blit_span;
        end
        2: begin
            blit_skip <= word_assign(blit_skip, din, wr);
            dout <= blit_skip;
        end
        3: begin
            blit_repeat <= word_assign(blit_repeat, din, wr);
            dout <= blit_repeat;
        end
        4: begin
            blit_start <= word_assign(blit_start, din, wr);
            dout <= blit_start;
        end
        7: begin
            if (wr[0]) begin
                blit_active <= 1;
                blit_next_addr <= blit_start;
                blit_span_count <= 0;
                blit_repeat_count <= 0;
            end
        end
        endcase
    end

end

endmodule