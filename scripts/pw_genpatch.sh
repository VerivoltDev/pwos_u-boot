#!/bin/bash

git diff v2023.04-09.02.00.005-phy2 board/phytec/phycore_am64x/phycore_am64x.env > PW_phycore_am64x.env.patch
git diff v2023.04-09.02.00.005-phy2 arch/arm/dts/k3-am642-phyboard-electra-rdk.dts > PW_k3-am642-phyboard-electra-rdk.dts.patch
git diff v2023.04-09.02.00.005-phy2 board/phytec/phycore_am64x/phycore-am64x.c > PW_phycore-am64x.c.patch
git diff v2023.04-09.02.00.005-phy2 configs/phycore_am64x_a53_defconfig > PW_phycore_am64x_a53_defconfig.patch

