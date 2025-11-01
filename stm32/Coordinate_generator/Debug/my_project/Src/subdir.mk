################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../my_project/Src/I2C_Coordinate_generator.c \
../my_project/Src/my_main.c 

OBJS += \
./my_project/Src/I2C_Coordinate_generator.o \
./my_project/Src/my_main.o 

C_DEPS += \
./my_project/Src/I2C_Coordinate_generator.d \
./my_project/Src/my_main.d 


# Each subdirectory must supply rules for building sources it contributes
my_project/Src/%.o my_project/Src/%.su my_project/Src/%.cyclo: ../my_project/Src/%.c my_project/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F756xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I"D:/rtg2024/embedded linux/final_project/Coordinate_generator/my_project/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-my_project-2f-Src

clean-my_project-2f-Src:
	-$(RM) ./my_project/Src/I2C_Coordinate_generator.cyclo ./my_project/Src/I2C_Coordinate_generator.d ./my_project/Src/I2C_Coordinate_generator.o ./my_project/Src/I2C_Coordinate_generator.su ./my_project/Src/my_main.cyclo ./my_project/Src/my_main.d ./my_project/Src/my_main.o ./my_project/Src/my_main.su

.PHONY: clean-my_project-2f-Src

