// license:BSD-3-Clause
// Derived from MAME TMS320C3x, copyright Aaron Giles.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <bit>
#include <algorithm>
#include <vector>
using offs_t=uint32_t;
namespace util {inline int32_t sext(uint32_t x,int n) {return (int32_t)(x<<(32-n))>>(32-n);}}
inline int64_t mul_32x32(int32_t a,int32_t b){return (int64_t)a*b;}
#define TMS320C3X_LOG_OPCODE_USAGE 0
#define BYTE_XOR_BE(x) ((x)^1)
#define BIT(v,b) (((v)>>(b))&1)
#define CHIP_TYPE_TMS320C32 2
#define ASSERT_LINE 1
#define CLEAR_LINE 0
#define CHIP_TYPE_TMS320C31 1
#define fatalerror(...) do {fprintf(stderr,__VA_ARGS__);abort();} while(0)
enum
{
	TMR_R0 = 0,
	TMR_R1,
	TMR_R2,
	TMR_R3,
	TMR_R4,
	TMR_R5,
	TMR_R6,
	TMR_R7,
	TMR_AR0,
	TMR_AR1,
	TMR_AR2,
	TMR_AR3,
	TMR_AR4,
	TMR_AR5,
	TMR_AR6,
	TMR_AR7,
	TMR_DP,
	TMR_IR0,
	TMR_IR1,
	TMR_BK,
	TMR_SP,
	TMR_ST,
	TMR_IE,
	TMR_IF,
	TMR_IOF,
	TMR_RS,
	TMR_RE,
	TMR_RC,
	TMR_R8,     // 3204x only
	TMR_R9,     // 3204x only
	TMR_R10,    // 3204x only
	TMR_R11,    // 3204x only
	TMR_TEMP1,  // used by the interpreter
	TMR_TEMP2,  // used by the interpreter
	TMR_TEMP3   // used by the interpreter
};

// flags
const int CFLAG     = 0x0001;
const int VFLAG     = 0x0002;
const int ZFLAG     = 0x0004;
const int NFLAG     = 0x0008;
const int UFFLAG    = 0x0010;
const int LVFLAG    = 0x0020;
const int LUFFLAG   = 0x0040;
const int OVMFLAG   = 0x0080;
const int RMFLAG    = 0x0100;
//const int CFFLAG    = 0x0400;
//const int CEFLAG    = 0x0800;
//const int CCFLAG    = 0x1000;
const int GIEFLAG   = 0x2000;



class tms320c3x_device {
public:
	struct tmsreg
	{
		// constructors
		tmsreg() { i32[0] = i32[1] = 0; }
		tmsreg(double value) { from_double(value); }
		tmsreg(int32_t mantissa, int8_t exponent) { set_mantissa(mantissa); set_exponent(exponent); }

		// getters
		uint32_t integer() const { return i32[0]; }
		int32_t mantissa() const { return i32[0]; }
		int8_t exponent() const { return i32[1]; }
		void set_mantissa(int32_t man) { i32[0] = man; }
		void set_exponent(int8_t exp) { i32[1] = exp; }

		// exporters
		float as_float() const;
		double as_double() const;

		// importers
		void from_double(double);

		uint32_t      i32[2];
	};
	// memory helpers
	uint32_t ROPCODE(offs_t pc);
	uint32_t RMEM(offs_t addr);
	void WMEM(offs_t addr, uint32_t data);

	// misc helpers
	void check_irqs();
	void execute_one();
	void update_special(int dreg);
	void burn_cycle(int cycle);
	bool condition(int which);

	// floating point helpers
	void int2float(tmsreg &srcdst);
	void float2int(tmsreg &srcdst, bool setflags);
	void negf(tmsreg &dst, tmsreg &src);
	void addf(tmsreg &dst, tmsreg &src1, tmsreg &src2);
	void subf(tmsreg &dst, tmsreg &src1, tmsreg &src2);
	void mpyf(tmsreg &dst, tmsreg &src1, tmsreg &src2);
	void norm(tmsreg &dst, tmsreg &src);

	// memory addressing
	uint32_t mod00_d(uint32_t op, uint8_t ar);
	uint32_t mod01_d(uint32_t op, uint8_t ar);
	uint32_t mod02_d(uint32_t op, uint8_t ar);
	uint32_t mod03_d(uint32_t op, uint8_t ar);
	uint32_t mod04_d(uint32_t op, uint8_t ar);
	uint32_t mod05_d(uint32_t op, uint8_t ar);
	uint32_t mod06_d(uint32_t op, uint8_t ar);
	uint32_t mod07_d(uint32_t op, uint8_t ar);

	uint32_t mod00_1(uint32_t op, uint8_t ar);
	uint32_t mod01_1(uint32_t op, uint8_t ar);
	uint32_t mod02_1(uint32_t op, uint8_t ar);
	uint32_t mod03_1(uint32_t op, uint8_t ar);
	uint32_t mod04_1(uint32_t op, uint8_t ar);
	uint32_t mod05_1(uint32_t op, uint8_t ar);
	uint32_t mod06_1(uint32_t op, uint8_t ar);
	uint32_t mod07_1(uint32_t op, uint8_t ar);

	uint32_t mod08(uint32_t op, uint8_t ar);
	uint32_t mod09(uint32_t op, uint8_t ar);
	uint32_t mod0a(uint32_t op, uint8_t ar);
	uint32_t mod0b(uint32_t op, uint8_t ar);
	uint32_t mod0c(uint32_t op, uint8_t ar);
	uint32_t mod0d(uint32_t op, uint8_t ar);
	uint32_t mod0e(uint32_t op, uint8_t ar);
	uint32_t mod0f(uint32_t op, uint8_t ar);

	uint32_t mod10(uint32_t op, uint8_t ar);
	uint32_t mod11(uint32_t op, uint8_t ar);
	uint32_t mod12(uint32_t op, uint8_t ar);
	uint32_t mod13(uint32_t op, uint8_t ar);
	uint32_t mod14(uint32_t op, uint8_t ar);
	uint32_t mod15(uint32_t op, uint8_t ar);
	uint32_t mod16(uint32_t op, uint8_t ar);
	uint32_t mod17(uint32_t op, uint8_t ar);

	uint32_t mod18(uint32_t op, uint8_t ar);
	uint32_t mod19(uint32_t op, uint8_t ar);
	uint32_t modillegal(uint32_t op, uint8_t ar);

	uint32_t mod00_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod01_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod02_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod03_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod04_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod05_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod06_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod07_1_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);

	uint32_t mod08_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod09_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod0a_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod0b_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod0c_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod0d_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod0e_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod0f_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);

	uint32_t mod10_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod11_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod12_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod13_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod14_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod15_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod16_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod17_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod18_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t mod19_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);
	uint32_t modillegal_def(uint32_t op, uint8_t ar, uint32_t *&defptrptr);

	// instructions
	void illegal(uint32_t op);
	void unimplemented(uint32_t op);

	void absf_reg(uint32_t op);
	void absf_dir(uint32_t op);
	void absf_ind(uint32_t op);
	void absf_imm(uint32_t op);
	void absi_reg(uint32_t op);
	void absi_dir(uint32_t op);
	void absi_ind(uint32_t op);
	void absi_imm(uint32_t op);
	void addc_reg(uint32_t op);
	void addc_dir(uint32_t op);
	void addc_ind(uint32_t op);
	void addc_imm(uint32_t op);
	void addf_reg(uint32_t op);
	void addf_dir(uint32_t op);
	void addf_ind(uint32_t op);
	void addf_imm(uint32_t op);
	void addi_reg(uint32_t op);
	void addi_dir(uint32_t op);
	void addi_ind(uint32_t op);
	void addi_imm(uint32_t op);
	void and_reg(uint32_t op);
	void and_dir(uint32_t op);
	void and_ind(uint32_t op);
	void and_imm(uint32_t op);
	void andn_reg(uint32_t op);
	void andn_dir(uint32_t op);
	void andn_ind(uint32_t op);
	void andn_imm(uint32_t op);
	void ash_reg(uint32_t op);
	void ash_dir(uint32_t op);
	void ash_ind(uint32_t op);
	void ash_imm(uint32_t op);
	void cmpf_reg(uint32_t op);
	void cmpf_dir(uint32_t op);
	void cmpf_ind(uint32_t op);
	void cmpf_imm(uint32_t op);
	void cmpi_reg(uint32_t op);
	void cmpi_dir(uint32_t op);
	void cmpi_ind(uint32_t op);
	void cmpi_imm(uint32_t op);
	void fix_reg(uint32_t op);
	void fix_dir(uint32_t op);
	void fix_ind(uint32_t op);
	void fix_imm(uint32_t op);
	void float_reg(uint32_t op);
	void float_dir(uint32_t op);
	void float_ind(uint32_t op);
	void float_imm(uint32_t op);
	void idle(uint32_t op);
	void lde_reg(uint32_t op);
	void lde_dir(uint32_t op);
	void lde_ind(uint32_t op);
	void lde_imm(uint32_t op);
	void ldf_reg(uint32_t op);
	void ldf_dir(uint32_t op);
	void ldf_ind(uint32_t op);
	void ldf_imm(uint32_t op);
	void ldfi_dir(uint32_t op);
	void ldfi_ind(uint32_t op);
	void ldi_reg(uint32_t op);
	void ldi_dir(uint32_t op);
	void ldi_ind(uint32_t op);
	void ldi_imm(uint32_t op);
	void ldii_dir(uint32_t op);
	void ldii_ind(uint32_t op);
	void ldm_reg(uint32_t op);
	void ldm_dir(uint32_t op);
	void ldm_ind(uint32_t op);
	void ldm_imm(uint32_t op);
	void lsh_reg(uint32_t op);
	void lsh_dir(uint32_t op);
	void lsh_ind(uint32_t op);
	void lsh_imm(uint32_t op);
	void mpyf_reg(uint32_t op);
	void mpyf_dir(uint32_t op);
	void mpyf_ind(uint32_t op);
	void mpyf_imm(uint32_t op);
	void mpyi_reg(uint32_t op);
	void mpyi_dir(uint32_t op);
	void mpyi_ind(uint32_t op);
	void mpyi_imm(uint32_t op);
	void negb_reg(uint32_t op);
	void negb_dir(uint32_t op);
	void negb_ind(uint32_t op);
	void negb_imm(uint32_t op);
	void negf_reg(uint32_t op);
	void negf_dir(uint32_t op);
	void negf_ind(uint32_t op);
	void negf_imm(uint32_t op);
	void negi_reg(uint32_t op);
	void negi_dir(uint32_t op);
	void negi_ind(uint32_t op);
	void negi_imm(uint32_t op);
	void nop_reg(uint32_t op);
	void nop_ind(uint32_t op);
	void norm_reg(uint32_t op);
	void norm_dir(uint32_t op);
	void norm_ind(uint32_t op);
	void norm_imm(uint32_t op);
	void not_reg(uint32_t op);
	void not_dir(uint32_t op);
	void not_ind(uint32_t op);
	void not_imm(uint32_t op);
	void pop(uint32_t op);
	void popf(uint32_t op);
	void push(uint32_t op);
	void pushf(uint32_t op);
	void or_reg(uint32_t op);
	void or_dir(uint32_t op);
	void or_ind(uint32_t op);
	void or_imm(uint32_t op);
	void maxspeed(uint32_t op);
	void rnd_reg(uint32_t op);
	void rnd_dir(uint32_t op);
	void rnd_ind(uint32_t op);
	void rnd_imm(uint32_t op);
	void rol(uint32_t op);
	void rolc(uint32_t op);
	void ror(uint32_t op);
	void rorc(uint32_t op);
	void rpts_reg(uint32_t op);
	void rpts_dir(uint32_t op);
	void rpts_ind(uint32_t op);
	void rpts_imm(uint32_t op);
	void stf_dir(uint32_t op);
	void stf_ind(uint32_t op);
	void stfi_dir(uint32_t op);
	void stfi_ind(uint32_t op);
	void sti_dir(uint32_t op);
	void sti_ind(uint32_t op);
	void stii_dir(uint32_t op);
	void stii_ind(uint32_t op);
	void sigi(uint32_t op);
	void subb_reg(uint32_t op);
	void subb_dir(uint32_t op);
	void subb_ind(uint32_t op);
	void subb_imm(uint32_t op);
	void subc_reg(uint32_t op);
	void subc_dir(uint32_t op);
	void subc_ind(uint32_t op);
	void subc_imm(uint32_t op);
	void subf_reg(uint32_t op);
	void subf_dir(uint32_t op);
	void subf_ind(uint32_t op);
	void subf_imm(uint32_t op);
	void subi_reg(uint32_t op);
	void subi_dir(uint32_t op);
	void subi_ind(uint32_t op);
	void subi_imm(uint32_t op);
	void subrb_reg(uint32_t op);
	void subrb_dir(uint32_t op);
	void subrb_ind(uint32_t op);
	void subrb_imm(uint32_t op);
	void subrf_reg(uint32_t op);
	void subrf_dir(uint32_t op);
	void subrf_ind(uint32_t op);
	void subrf_imm(uint32_t op);
	void subri_reg(uint32_t op);
	void subri_dir(uint32_t op);
	void subri_ind(uint32_t op);
	void subri_imm(uint32_t op);
	void tstb_reg(uint32_t op);
	void tstb_dir(uint32_t op);
	void tstb_ind(uint32_t op);
	void tstb_imm(uint32_t op);
	void xor_reg(uint32_t op);
	void xor_dir(uint32_t op);
	void xor_ind(uint32_t op);
	void xor_imm(uint32_t op);
	void iack_dir(uint32_t op);
	void iack_ind(uint32_t op);
	void addc3_regreg(uint32_t op);
	void addc3_indreg(uint32_t op);
	void addc3_regind(uint32_t op);
	void addc3_indind(uint32_t op);
	void addf3_regreg(uint32_t op);
	void addf3_indreg(uint32_t op);
	void addf3_regind(uint32_t op);
	void addf3_indind(uint32_t op);
	void addi3_regreg(uint32_t op);
	void addi3_indreg(uint32_t op);
	void addi3_regind(uint32_t op);
	void addi3_indind(uint32_t op);
	void and3_regreg(uint32_t op);
	void and3_indreg(uint32_t op);
	void and3_regind(uint32_t op);
	void and3_indind(uint32_t op);
	void andn3_regreg(uint32_t op);
	void andn3_indreg(uint32_t op);
	void andn3_regind(uint32_t op);
	void andn3_indind(uint32_t op);
	void ash3_regreg(uint32_t op);
	void ash3_indreg(uint32_t op);
	void ash3_regind(uint32_t op);
	void ash3_indind(uint32_t op);
	void cmpf3_regreg(uint32_t op);
	void cmpf3_indreg(uint32_t op);
	void cmpf3_regind(uint32_t op);
	void cmpf3_indind(uint32_t op);
	void cmpi3_regreg(uint32_t op);
	void cmpi3_indreg(uint32_t op);
	void cmpi3_regind(uint32_t op);
	void cmpi3_indind(uint32_t op);
	void lsh3_regreg(uint32_t op);
	void lsh3_indreg(uint32_t op);
	void lsh3_regind(uint32_t op);
	void lsh3_indind(uint32_t op);
	void mpyf3_regreg(uint32_t op);
	void mpyf3_indreg(uint32_t op);
	void mpyf3_regind(uint32_t op);
	void mpyf3_indind(uint32_t op);
	void mpyi3_regreg(uint32_t op);
	void mpyi3_indreg(uint32_t op);
	void mpyi3_regind(uint32_t op);
	void mpyi3_indind(uint32_t op);
	void or3_regreg(uint32_t op);
	void or3_indreg(uint32_t op);
	void or3_regind(uint32_t op);
	void or3_indind(uint32_t op);
	void subb3_regreg(uint32_t op);
	void subb3_indreg(uint32_t op);
	void subb3_regind(uint32_t op);
	void subb3_indind(uint32_t op);
	void subf3_regreg(uint32_t op);
	void subf3_indreg(uint32_t op);
	void subf3_regind(uint32_t op);
	void subf3_indind(uint32_t op);
	void subi3_regreg(uint32_t op);
	void subi3_indreg(uint32_t op);
	void subi3_regind(uint32_t op);
	void subi3_indind(uint32_t op);
	void tstb3_regreg(uint32_t op);
	void tstb3_indreg(uint32_t op);
	void tstb3_regind(uint32_t op);
	void tstb3_indind(uint32_t op);
	void xor3_regreg(uint32_t op);
	void xor3_indreg(uint32_t op);
	void xor3_regind(uint32_t op);
	void xor3_indind(uint32_t op);
	void ldfu_reg(uint32_t op);
	void ldfu_dir(uint32_t op);
	void ldfu_ind(uint32_t op);
	void ldfu_imm(uint32_t op);
	void ldflo_reg(uint32_t op);
	void ldflo_dir(uint32_t op);
	void ldflo_ind(uint32_t op);
	void ldflo_imm(uint32_t op);
	void ldfls_reg(uint32_t op);
	void ldfls_dir(uint32_t op);
	void ldfls_ind(uint32_t op);
	void ldfls_imm(uint32_t op);
	void ldfhi_reg(uint32_t op);
	void ldfhi_dir(uint32_t op);
	void ldfhi_ind(uint32_t op);
	void ldfhi_imm(uint32_t op);
	void ldfhs_reg(uint32_t op);
	void ldfhs_dir(uint32_t op);
	void ldfhs_ind(uint32_t op);
	void ldfhs_imm(uint32_t op);
	void ldfeq_reg(uint32_t op);
	void ldfeq_dir(uint32_t op);
	void ldfeq_ind(uint32_t op);
	void ldfeq_imm(uint32_t op);
	void ldfne_reg(uint32_t op);
	void ldfne_dir(uint32_t op);
	void ldfne_ind(uint32_t op);
	void ldfne_imm(uint32_t op);
	void ldflt_reg(uint32_t op);
	void ldflt_dir(uint32_t op);
	void ldflt_ind(uint32_t op);
	void ldflt_imm(uint32_t op);
	void ldfle_reg(uint32_t op);
	void ldfle_dir(uint32_t op);
	void ldfle_ind(uint32_t op);
	void ldfle_imm(uint32_t op);
	void ldfgt_reg(uint32_t op);
	void ldfgt_dir(uint32_t op);
	void ldfgt_ind(uint32_t op);
	void ldfgt_imm(uint32_t op);
	void ldfge_reg(uint32_t op);
	void ldfge_dir(uint32_t op);
	void ldfge_ind(uint32_t op);
	void ldfge_imm(uint32_t op);
	void ldfnv_reg(uint32_t op);
	void ldfnv_dir(uint32_t op);
	void ldfnv_ind(uint32_t op);
	void ldfnv_imm(uint32_t op);
	void ldfv_reg(uint32_t op);
	void ldfv_dir(uint32_t op);
	void ldfv_ind(uint32_t op);
	void ldfv_imm(uint32_t op);
	void ldfnuf_reg(uint32_t op);
	void ldfnuf_dir(uint32_t op);
	void ldfnuf_ind(uint32_t op);
	void ldfnuf_imm(uint32_t op);
	void ldfuf_reg(uint32_t op);
	void ldfuf_dir(uint32_t op);
	void ldfuf_ind(uint32_t op);
	void ldfuf_imm(uint32_t op);
	void ldfnlv_reg(uint32_t op);
	void ldfnlv_dir(uint32_t op);
	void ldfnlv_ind(uint32_t op);
	void ldfnlv_imm(uint32_t op);
	void ldflv_reg(uint32_t op);
	void ldflv_dir(uint32_t op);
	void ldflv_ind(uint32_t op);
	void ldflv_imm(uint32_t op);
	void ldfnluf_reg(uint32_t op);
	void ldfnluf_dir(uint32_t op);
	void ldfnluf_ind(uint32_t op);
	void ldfnluf_imm(uint32_t op);
	void ldfluf_reg(uint32_t op);
	void ldfluf_dir(uint32_t op);
	void ldfluf_ind(uint32_t op);
	void ldfluf_imm(uint32_t op);
	void ldfzuf_reg(uint32_t op);
	void ldfzuf_dir(uint32_t op);
	void ldfzuf_ind(uint32_t op);
	void ldfzuf_imm(uint32_t op);
	void ldiu_reg(uint32_t op);
	void ldiu_dir(uint32_t op);
	void ldiu_ind(uint32_t op);
	void ldiu_imm(uint32_t op);
	void ldilo_reg(uint32_t op);
	void ldilo_dir(uint32_t op);
	void ldilo_ind(uint32_t op);
	void ldilo_imm(uint32_t op);
	void ldils_reg(uint32_t op);
	void ldils_dir(uint32_t op);
	void ldils_ind(uint32_t op);
	void ldils_imm(uint32_t op);
	void ldihi_reg(uint32_t op);
	void ldihi_dir(uint32_t op);
	void ldihi_ind(uint32_t op);
	void ldihi_imm(uint32_t op);
	void ldihs_reg(uint32_t op);
	void ldihs_dir(uint32_t op);
	void ldihs_ind(uint32_t op);
	void ldihs_imm(uint32_t op);
	void ldieq_reg(uint32_t op);
	void ldieq_dir(uint32_t op);
	void ldieq_ind(uint32_t op);
	void ldieq_imm(uint32_t op);
	void ldine_reg(uint32_t op);
	void ldine_dir(uint32_t op);
	void ldine_ind(uint32_t op);
	void ldine_imm(uint32_t op);
	void ldilt_reg(uint32_t op);
	void ldilt_dir(uint32_t op);
	void ldilt_ind(uint32_t op);
	void ldilt_imm(uint32_t op);
	void ldile_reg(uint32_t op);
	void ldile_dir(uint32_t op);
	void ldile_ind(uint32_t op);
	void ldile_imm(uint32_t op);
	void ldigt_reg(uint32_t op);
	void ldigt_dir(uint32_t op);
	void ldigt_ind(uint32_t op);
	void ldigt_imm(uint32_t op);
	void ldige_reg(uint32_t op);
	void ldige_dir(uint32_t op);
	void ldige_ind(uint32_t op);
	void ldige_imm(uint32_t op);
	void ldinv_reg(uint32_t op);
	void ldinv_dir(uint32_t op);
	void ldinv_ind(uint32_t op);
	void ldinv_imm(uint32_t op);
	void ldiuf_reg(uint32_t op);
	void ldiuf_dir(uint32_t op);
	void ldiuf_ind(uint32_t op);
	void ldiuf_imm(uint32_t op);
	void ldinuf_reg(uint32_t op);
	void ldinuf_dir(uint32_t op);
	void ldinuf_ind(uint32_t op);
	void ldinuf_imm(uint32_t op);
	void ldiv_reg(uint32_t op);
	void ldiv_dir(uint32_t op);
	void ldiv_ind(uint32_t op);
	void ldiv_imm(uint32_t op);
	void ldinlv_reg(uint32_t op);
	void ldinlv_dir(uint32_t op);
	void ldinlv_ind(uint32_t op);
	void ldinlv_imm(uint32_t op);
	void ldilv_reg(uint32_t op);
	void ldilv_dir(uint32_t op);
	void ldilv_ind(uint32_t op);
	void ldilv_imm(uint32_t op);
	void ldinluf_reg(uint32_t op);
	void ldinluf_dir(uint32_t op);
	void ldinluf_ind(uint32_t op);
	void ldinluf_imm(uint32_t op);
	void ldiluf_reg(uint32_t op);
	void ldiluf_dir(uint32_t op);
	void ldiluf_ind(uint32_t op);
	void ldiluf_imm(uint32_t op);
	void ldizuf_reg(uint32_t op);
	void ldizuf_dir(uint32_t op);
	void ldizuf_ind(uint32_t op);
	void ldizuf_imm(uint32_t op);
	void execute_delayed(uint32_t newpc);
	void br_imm(uint32_t op);
	void brd_imm(uint32_t op);
	void call_imm(uint32_t op);
	void rptb_imm(uint32_t op);
	void swi(uint32_t op);
	void brc_reg(uint32_t op);
	void brcd_reg(uint32_t op);
	void brc_imm(uint32_t op);
	void brcd_imm(uint32_t op);
	void dbc_reg(uint32_t op);
	void dbcd_reg(uint32_t op);
	void dbc_imm(uint32_t op);
	void dbcd_imm(uint32_t op);
	void callc_reg(uint32_t op);
	void callc_imm(uint32_t op);
	void trap(int trapnum);
	void trapc(uint32_t op);
	void retic_reg(uint32_t op);
	void retsc_reg(uint32_t op);
	void mpyaddf_0(uint32_t op);
	void mpyaddf_1(uint32_t op);
	void mpyaddf_2(uint32_t op);
	void mpyaddf_3(uint32_t op);
	void mpysubf_0(uint32_t op);
	void mpysubf_1(uint32_t op);
	void mpysubf_2(uint32_t op);
	void mpysubf_3(uint32_t op);
	void mpyaddi_0(uint32_t op);
	void mpyaddi_1(uint32_t op);
	void mpyaddi_2(uint32_t op);
	void mpyaddi_3(uint32_t op);
	void mpysubi_0(uint32_t op);
	void mpysubi_1(uint32_t op);
	void mpysubi_2(uint32_t op);
	void mpysubi_3(uint32_t op);
	void stfstf(uint32_t op);
	void stisti(uint32_t op);
	void ldfldf(uint32_t op);
	void ldildi(uint32_t op);
	void absfstf(uint32_t op);
	void absisti(uint32_t op);
	void addf3stf(uint32_t op);
	void addi3sti(uint32_t op);
	void and3sti(uint32_t op);
	void ash3sti(uint32_t op);
	void fixsti(uint32_t op);
	void floatstf(uint32_t op);
	void ldfstf(uint32_t op);
	void ldisti(uint32_t op);
	void lsh3sti(uint32_t op);
	void mpyf3stf(uint32_t op);
	void mpyi3sti(uint32_t op);
	void negfstf(uint32_t op);
	void negisti(uint32_t op);
	void notsti(uint32_t op);
	void or3sti(uint32_t op);
	void subf3stf(uint32_t op);
	void subi3sti(uint32_t op);
	void xor3sti(uint32_t op);

	union int_double
	{
		double d;
		float f[2];
		uint32_t i[2];
	};

	// core registers
	uint32_t            m_pc;
	tmsreg              m_r[36];
	uint32_t            m_bkmask;

	// tables
	static void (tms320c3x_device::*const s_tms320c3x_ops[])(uint32_t op);
	static uint32_t (tms320c3x_device::*const s_indirect_d[0x20])(uint32_t, uint8_t);
	static uint32_t (tms320c3x_device::*const s_indirect_1[0x20])(uint32_t, uint8_t);
	static uint32_t (tms320c3x_device::*const s_indirect_1_def[0x20])(uint32_t, uint8_t, uint32_t *&);

 uint16_t m_irq_state=0;bool m_delayed=false,m_irq_pending=false,m_is_idling=false,m_is_lopower=false;
 int m_icount=0,m_chip_type=1;
 struct Callback {void operator()(uint32_t,int){}} m_xf0_cb,m_xf1_cb,m_iack_cb;
 bool debugger_enabled(){return false;}
 void debugger_instruction_hook(uint32_t){}
 uint32_t ram[0x10000]={},internal[0x800]={},io[256]={};
 std::vector<uint8_t> boot;
 std::vector<uint32_t> rom;
 std::vector<int16_t> pcm;
 int64_t ack_deadline=-1;
 int64_t cycles=0,dma_deadline=-1,timer_deadline[2]={-1,-1};
 uint32_t dma_end=0;int command=0,rate=0;bool pending=false;
 void run(int count);
 void irq(int n,bool value);
 void command_write(int n);
 void dma();
 void boot_load(const char *dir);
};
