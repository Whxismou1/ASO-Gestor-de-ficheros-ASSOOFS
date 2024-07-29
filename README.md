# ASSOO - Práctica Final: Implementación del Sistema de Ficheros ASSOOFS

## Descripción

Este repositorio contiene la práctica final para la asignatura "Ampliación de Sistemas Operativos" del segundo curso de Ingeniería Informática en la Universidad de Leon. El proyecto consiste en la implementación y creación de un sistema de ficheros denominado ASSOOFS. ASSOOFS es un sistema de ficheros básico diseñado para gestionar la organización y almacenamiento de datos en dispositivos de bloques(Kernel).

## Instalación

Para compilar y ejecutar el sistema de ficheros ASSOOFS, sigue estos pasos:

1. **Clona el repositorio:**
   ```bash
   git clone https://github.com/Whxismou1/ASO-Gestor-de-ficheros-ASSOOFS.git
   ```
2. Accede al directorio del proyecto:
   ```bash
   cd ASO-Gestor-de-ficheros-ASSOOFS
   ```
3. Compila el proyecto:
   ```bash
   make
   ```
## Uso y comprobación
Para utilizar el sistema de ficheros ASSOOFS, puedes emplear los siguientes comandos (despues del comando make):
   ```bash
    dd bs=4096 count=100 if=/dev/zero of=image

    ./mkassoofs image

    insmod assoofs.ko

    mkdir mnt

    mount -o loop -t assoofs image mnt

    cd mnt/

    ls
    -Salida esperada: README.txt

    cat README.txt
    -Salida esperada: Hola mundo, os saludo desde un sistema de ficheros ASSOOFS.

    cp README.txt README.txt.bak

    ls
    -Salida esperada: README.txt README.txt.bak

    cat README.txt.bak
    -Salida esperada: Hola mundo, os saludo desde un sistema de ficheros ASSOOFS.

    mkdir tmp

    ls
    -Salida esperada: README.txt README.txt.bak tmp

    cp README.txt tmp/HOLA

    cat tmp/HOLA
    -Salida esperada: Hola mundo, os saludo desde un sistema de ficheros ASSOOFS.

    cd ..

    umount mnt/

    mount -o loop -t assoofs image mnt/

    ls -l mnt/
    -Salida esperada:
    total 0
    ---------- 1 root root 0 May 8 13:14 README.txt
    ---------- 1 root root 0 May 8 13:14 README.txt.bak
    drwxr-xr-x 1 root root 0 May 8 13:14 tmp

    umount mnt/

    rmmod assoofs
   ```
## Notas
- Se han implementado las partes básicas y las opcionales exceptuando el mv (En caso de querer implementarlo es usando el cp & el rm)
- Por facilidad una vez que se monte el sistema, por defecto se introduce por defecto el archivo README.txt
