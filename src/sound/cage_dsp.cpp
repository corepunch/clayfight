#include "cage_dsp.hpp"
#define IREG(rnum) (m_r[rnum].i32[0])
float tms320c3x_device::tmsreg::as_float() const
{
	int_double id;

	// map 0 to 0
	if (mantissa() == 0 && exponent() == -128)
		return 0;

	// handle positive numbers
	else if (mantissa() >= 0)
	{
		int exp = (exponent() + 127) << 23;
		id.i[0] = exp + (mantissa() >> 8);
	}

	// handle negative numbers
	else
	{
		int exp = (exponent() + 127) << 23;
		int32_t man = -mantissa();
		id.i[0] = 0x80000000 + exp + ((man >> 8) & 0x00ffffff);
	}

	// return the converted float
	return id.f[0];
}


//-------------------------------------------------
//  as_double - interpret the contents of a tmsreg
//  as a DSP-encoded floating-point value, and
//  extract a 64-bit IEEE double from it
//-------------------------------------------------

double tms320c3x_device::tmsreg::as_double() const
{
	int_double id;

	// map 0 to 0
	if (mantissa() == 0 && exponent() == -128)
		return 0;

	// handle positive numbers
	else if (mantissa() >= 0)
	{
		int exp = (exponent() + 1023) << 20;
		id.i[BYTE_XOR_BE(0)] = exp + (mantissa() >> 11);
		id.i[BYTE_XOR_BE(1)] = (mantissa() << 21) & 0xffe00000;
	}

	// handle negative numbers
	else
	{
		int exp = (exponent() + 1023) << 20;
		int32_t man = -mantissa();
		id.i[BYTE_XOR_BE(0)] = 0x80000000 + exp + ((man >> 11) & 0x001fffff);
		id.i[BYTE_XOR_BE(1)] = (man << 21) & 0xffe00000;
	}

	// return the converted double
	return id.d;
}


//-------------------------------------------------
//  from_double - import a 64-bit IEEE double into
//  the DSP's internal floating point format
//-------------------------------------------------

void tms320c3x_device::tmsreg::from_double(double val)
{
	// extract mantissa and exponent from the IEEE input
	int_double id;
	id.d = val;
	int32_t mantissa = ((id.i[BYTE_XOR_BE(0)] & 0x000fffff) << 11) | ((id.i[BYTE_XOR_BE(1)] & 0xffe00000) >> 21);
	int32_t exponent = ((id.i[BYTE_XOR_BE(0)] & 0x7ff00000) >> 20) - 1023;

	// if we're too small, map to 0
	if (exponent < -128)
	{
		set_mantissa(0);
		set_exponent(-128);
	}

	// if we're too large, map to the maximum value
	else if (exponent > 127)
	{
		if ((int32_t)id.i[BYTE_XOR_BE(0)] >= 0)
			set_mantissa(0x7fffffff);
		else
			set_mantissa(0x80000001);
		set_exponent(127);
	}

	// if we're positive, map directly
	else if ((int32_t)id.i[BYTE_XOR_BE(0)] >= 0)
	{
		set_mantissa(mantissa);
		set_exponent(exponent);
	}

	// if we're negative with a non-zero mantissa, remove the leading sign bit
	else if (mantissa != 0)
	{
		set_mantissa(0x80000000 | -mantissa);
		set_exponent(exponent);
	}

	// if we're negative with a zero mantissa, normalize
	else
	{
		set_mantissa(0x80000000);
		set_exponent(exponent - 1);
	}
}



void tms320c3x_device::check_irqs()
{
	// determine if we have any live interrupts
	uint16_t validints = IREG(TMR_IF) & IREG(TMR_IE) & 0x0fff;
	if (validints == 0 || (IREG(TMR_ST) & GIEFLAG) == 0)
		return;

	// find the lowest signalled value
	int whichtrap = 0;
	for (int i = 0; i < 12; i++)
		if (validints & (1 << i))
		{
			whichtrap = i + 1;
			break;
		}

	// no longer idling if we get here
	m_is_idling = false;
	if (!m_delayed)
	{
		uint16_t intmask = 1 << (whichtrap - 1);


		// bit in IF is cleared when interrupt is taken
		IREG(TMR_IF) &= ~intmask;
		trap(whichtrap);

		// after auto-clearing the interrupt bit, we need to re-trigger
		// level-sensitive interrupts
		if (m_chip_type == CHIP_TYPE_TMS320C31 || (IREG(TMR_ST) & 0x4000) == 0)
			IREG(TMR_IF) |= m_irq_state & 0x0f;
	}
	else
		m_irq_pending = true;
}



#include "cage_ops.ipp"
