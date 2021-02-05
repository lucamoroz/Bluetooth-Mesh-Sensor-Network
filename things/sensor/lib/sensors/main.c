#include "ccs811.h"
#include "hts221.h"
#include "lps22hb.h"

void main(void)
{
  const struct device *ccs811 = ccs811_setup();
  const struct device *hts221 = hts221_setup();
  const struct device *lps22hb = lps22hb_setup();

  if (ccs811 != NULL && hts221 != NULL) {
    while (true) {
      hts221_handler(hts221);
      lps22hb_handler(lps22hb);
      k_msleep(60000);
    }
  }
}
