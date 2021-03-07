# build kernel module

## Within CMake project (opt.A)
```bash
cmake [path-to-main-cmake-project]
make module
```

## make (opt.B)

```bash
make
```

# load kernel module

```bash
insmod alteldma.ko
```
