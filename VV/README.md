# PW U-boot

## How to get
 - To clone only the PW U-Boot branch use this command:
```
git clone -b pw/v2024.05 --single-branch git@github.com:VerivoltDev/pwos_u-boot.git
```

## How to generate the diff files
 - Use this script:
```
./VV/scripts/pw_genpatch.sh
```
Note: The files will be saved on: `./VV/patches`
