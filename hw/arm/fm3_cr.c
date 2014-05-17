/*
 * Fujitsu FM3 Clock/Reset
 *
 * Copyright (c) 2012 Pylone, Inc.
 * Written by Masashi YOKOTA <yokota@pylone.jp>
 *
 * This code is licensed under the GNU GPL v2.
 */
#include "hw/sysbus.h"
#include "hw/devices.h"
#include "hw/arm/arm.h"


#define FM3_CR(obj) \
    OBJECT_CHECK(Fm3CrState, (obj), TYPE_FM3_CLK_RST)	/* obj��Fm3CrState�ŃL���X�g���Ă����v���m�F����B */

/*
 ***
  ���̃��W���[�����T�|�[�g����n�[�h�E�F�A���̒�`
  */

/* �n�[�h�E�F�A���� */
#define TYPE_FM3_CLK_RST	"fm3.cr"
/* ���W�X�^�}�N�� */
#define FM3_CR_SCM_CTL_OFFSET       (0x0000)
#define FM3_CR_SCM_STR_OFFSET       (0x0004)
#define FM3_CR_STB_CTL_OFFSET       (0x0008)
#define FM3_CR_RST_STR_OFFSET       (0x000c)
#define FM3_CR_BSC_PSR_OFFSET       (0x0010)
#define FM3_CR_APBC0_PSR_OFFSET     (0x0014)
#define FM3_CR_APBC1_PSR_OFFSET     (0x0018)
#define FM3_CR_APBC2_PSR_OFFSET     (0x001c)
#define FM3_CR_SWC_PSR_OFFSET       (0x0020)
#define FM3_CR_TTC_PSR_OFFSET       (0x0028)
#define FM3_CR_CSW_TMR_OFFSET       (0x0030)
#define FM3_CR_PSW_TMR_OFFSET       (0x0034)
#define FM3_CR_PLL_CTL1_OFFSET      (0x0038)
#define FM3_CR_PLL_CTL2_OFFSET      (0x003c)
#define FM3_CR_CSV_CTL_OFFSET       (0x0040)
#define FM3_CR_CSV_STR_OFFSET       (0x0044)
#define FM3_CR_FCSWH_CTL_OFFSET     (0x0048)
#define FM3_CR_FCSWL_CTL_OFFSET     (0x004c)
#define FM3_CR_FCSWD_CTL_OFFSET     (0x0050)
#define FM3_CR_DBWDT_CTL_OFFSET     (0x0054)
#define FM3_CR_INT_ENR_OFFSET       (0x0060)
#define FM3_CR_INT_STR_OFFSET       (0x0064)
#define FM3_CR_INT_CLR_OFFSET       (0x0068)

#define FM3_CR_HI_OSC_HZ            (4000000)
#define FM3_CR_LOW_OSC_HZ           (100000)

/* �����I�ȃn�[�h�E�F�A���\���� */
typedef struct {
	/* �K�{���� */
    SysBusDevice busdev;
    MemoryRegion mmio;
	/* �����I�Ɏ����Ă����������ȂǁB�i���W�X�^�Ƃ��j */
	uint32_t scm;
    uint32_t bsc;
    uint32_t pll1;
    uint32_t pll2;
    uint32_t main_clk_hz;
    uint32_t sub_clk_hz;
    uint32_t master_clk_hz;
} Fm3CrState;

/* ���������֐� */
static uint32_t fm3_cr_get_pll(Fm3CrState *s)
{
    uint32_t k = ((s->pll1 >> 4) & 0xf) + 1;
    uint32_t n = (s->pll2 & 0x3f) + 1;
    return s->main_clk_hz / k * n;
}

static void fm3_cr_update_system_clock(Fm3CrState *s)
{
    int tmp = system_clock_scale;
    switch ((s->scm >> 5) & 3) {
    case 0:
        s->master_clk_hz = FM3_CR_HI_OSC_HZ;
        break;
    case 1:
        if (s->scm & (1 << 3))
            s->master_clk_hz = s->main_clk_hz;
        else
            s->master_clk_hz = 0;
        break;
    case 2:
        if (s->scm & (1 << 4))
            s->master_clk_hz = fm3_cr_get_pll(s);
        else
            s->master_clk_hz = 0;
        break;
    case 4:
        s->master_clk_hz = FM3_CR_LOW_OSC_HZ;
        break;
    case 5:
        if (s->scm & (1 << 3))
            s->master_clk_hz = s->main_clk_hz;
        else
            s->master_clk_hz = 0;
        break;
    default:
        printf("FM3_CR: Invalid selection for the master clock: SCM_CTL=0x%x\n", s->scm);
        return;

    }
    switch (s->bsc) {
    case 0:
        system_clock_scale = s->master_clk_hz;
        break;
    case 1:
        system_clock_scale = s->master_clk_hz >> 1;
        break;
    case 2:
        system_clock_scale = s->master_clk_hz / 3;
        break;
    case 3:
        system_clock_scale = s->master_clk_hz >> 2;
        break;
    case 4:
        system_clock_scale = s->master_clk_hz / 6;
        break;
    case 5:
        system_clock_scale = s->master_clk_hz >> 3;
        break;
    case 6:
        system_clock_scale = s->master_clk_hz >> 4;
        break;
    default:
        printf("FM3_CR: Invalid divisor setting for the base clock: BSC_PSR=0x%x\n", s->bsc);
        return;
    }

    if (tmp != system_clock_scale)
        printf("FM3_CR: Base clock at %d Hz\n", system_clock_scale);
}

/*
 -- ���[�h�������ɌĂ΂��֐� --
 *opaque	: memory_region_init_io�̑�4����
 offset		: �A�h���X
 size		: �A�N�Z�X�T�C�Y
 return		: �ǂݏo�����f�[�^
 */
static uint64_t fm3_cr_read(void *opaque, hwaddr offset,
                            unsigned size)
{
    Fm3CrState *s = (Fm3CrState *)opaque;
    uint64_t retval = 0;

    switch (offset) {
    case FM3_CR_SCM_STR_OFFSET:
    case FM3_CR_SCM_CTL_OFFSET:
        retval = s->scm;
        break;
    case FM3_CR_BSC_PSR_OFFSET:
        retval = s->bsc;
        break;
    case FM3_CR_PLL_CTL1_OFFSET:
        retval = s->pll1;
        break;
    case FM3_CR_PLL_CTL2_OFFSET:
        retval = s->pll2;
        break;
    default:
        break;
    }
    return retval;
}

/*
 -- ���C�g�������ɌĂ΂��֐� --
 *opaque	: memory_region_init_io�̑�4����
 offset		: �A�h���X
 value		: �f�[�^
 size		: �A�N�Z�X�T�C�Y
 */
static void fm3_cr_write(void *opaque, hwaddr offset,
                         uint64_t value, unsigned size)
{
    Fm3CrState *s = (Fm3CrState *)opaque;
    switch (offset) {
    case FM3_CR_SCM_CTL_OFFSET:
        s->scm = value & 0xff;
        break;
    case FM3_CR_BSC_PSR_OFFSET:
        s->bsc = value & 3;
        break;
    case FM3_CR_PLL_CTL1_OFFSET:
        s->pll1 = value;
        break;
    case FM3_CR_PLL_CTL2_OFFSET:
        s->pll2 = value & 0x3f;
        if (49 < s->pll2) {
            printf("FM3_CR: Invalid pll feedback divisor: PLLN=%d\n", s->pll2);
            return;
        }
        break;
    default:
        return;
    }
    fm3_cr_update_system_clock(s);
}

/* �����Œ�`���ꂽ�֐���QEMU��CPU����Ă΂��B */
static const MemoryRegionOps fm3_cr_mem_ops = {
    .read = fm3_cr_read,				/* ���[�h�������ɌĂ΂�� */
    .write = fm3_cr_write,				/* ���C�g�������ɌĂ΂�� */
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* ���Z�b�g�֐� */
static void fm3_cr_reset(DeviceState *d)
{
    Fm3CrState *s = FM3_CR(d);
    s->master_clk_hz = FM3_CR_HI_OSC_HZ;
}

/* �������B�n�[�h�E�F�A���������ꂽ�Ƃ��ɌĂ΂��B */
static int fm3_cr_init(SysBusDevice *dev)
{
	DeviceState	*devs	= DEVICE(dev);
    Fm3CrState	*s		= FM3_CR(devs);

    memory_region_init_io(&s->mmio, OBJECT(s), &fm3_cr_mem_ops, s, TYPE_FM3_CLK_RST, 0x1000);
    sysbus_init_mmio(dev, &s->mmio);

    system_clock_scale = s->main_clk_hz;
    return 0;
}

/* ������� */
static Property fm3_cr_properties[] = {
    DEFINE_PROP_UINT32("pll1"			, Fm3CrState	, pll1			, 0),
    DEFINE_PROP_UINT32("pll2"			, Fm3CrState	, pll2			, 0),
    DEFINE_PROP_UINT32("main_clk_hz"	, Fm3CrState	, main_clk_hz	, 4000000),
    DEFINE_PROP_UINT32("sub_clk_hz"		, Fm3CrState	, sub_clk_hz	, 32768),
    DEFINE_PROP_UINT32("master_clk_hz"	, Fm3CrState	, master_clk_hz	, 0),
    DEFINE_PROP_END_OF_LIST(),
};

/* �������B�n�[�h�E�F�A�o�^����QEMU����Ă΂��B */
static void fm3_cr_class_init(ObjectClass *klass, void *data)
{
	DeviceClass			*dc	= DEVICE_CLASS(klass);
	SysBusDeviceClass	*k	= SYS_BUS_DEVICE_CLASS(klass);

	k->init		= fm3_cr_init;			/* �������֐���o�^		*/
	dc->desc	= TYPE_FM3_CLK_RST;		/* �n�[�h�E�F�A����		*/
	dc->reset	= fm3_cr_reset;			/* ���Z�b�g���ɌĂ΂�� */
	dc->props	= fm3_cr_properties;	/* �������				*/
}

/* �n�[�h�E�F�A���B */
static const TypeInfo fm3_cr_info = {
	.name			= TYPE_FM3_CLK_RST,		/* �n�[�h�E�F�A����		*/
	.parent			= TYPE_SYS_BUS_DEVICE,	/* �ڑ��o�X				*/
	.instance_size	= sizeof(Fm3CrState),	/* �C���X�^���X�T�C�Y	*/
	.class_init		= fm3_cr_class_init,	/* �������֐��̃|�C���^	*/
};

/* �n�[�h�E�F�A����QEMU�ɓo�^�B */
static void fm3_register_devices(void)
{
    type_register_static(&fm3_cr_info);
}

type_init(fm3_register_devices)
