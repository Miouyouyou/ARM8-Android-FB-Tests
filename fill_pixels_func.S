
.text
/* Arguments :
 * - x0 : Address of the buffer
 */
.globl fill_pixels
.func fill_pixels
fill_pixels:
	ldpsw x1, x2, [x0] // x1 <- buffer_width,  x2 <- buffer_height
	ldpsw x3, x4, [x0, 8] // x3 <- buffer_stride, x4 <- buffer_format
	ldr x0, [x0, 16]     // x0 <- buffer_pixels

	mov x5, 0x379
	// Store the color 8 times in a 128 bits register
	// That way, saving the vector register in the framebuffer will
	// set up to 8 pixels at a time.
	// ("Up to" since saving the register in the ]width,stride] area
	//  will lit nothing)
	ins v0.h[0], w5
	ins v0.h[1], w5
	ins v0.h[2], w5
	ins v0.h[3], w5
	ins v0.h[4], w5
	ins v0.h[5], w5
	ins v0.h[6], w5
	ins v0.h[7], w5
	ins v1.d[0], v0.d[0]
	ins v1.d[1], v0.d[1]
	ins v2.d[0], v0.d[0]
	ins v2.d[1], v0.d[1]
	ins v3.d[0], v0.d[0]
	ins v3.d[1], v0.d[1]
	mov x9, 64 // post-index offset
	mov x10, x0 // current_pixel
	mov x12, 1920
.Lline_loop_start:
	sub x12, x12, 1
	mov x11, 4
.Lpixel_pack_loop_start:
	sub x11, x11, 1
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x10], 64 // 32 pixels lit
	st1 {v0.8h, v1.8h}, [x10], 32 // 16 pixels lit
	cbnz x11, .Lpixel_pack_loop_start
	add x0, x0, x3, lsl #1
	mov x10, x0
	cbnz x12, .Lline_loop_start
	ret
