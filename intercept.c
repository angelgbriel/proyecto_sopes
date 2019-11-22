#include <linux/version.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <asm/unistd.h>

MODULE_LICENSE("GPL");

// IOCTL commands
#define IOCTL_PATCH_TABLE 0x00000001
#define IOCTL_FIX_TABLE 0x00000004



//direccion de systable, se encuentra con el siguiente comando: cat /proc/kallsyms  | grep sys_call
// tomar la direccion de sys_call_table
//realizar cada vez que se reinicie el sistema puesto que la direccion cambia

unsigned long *sys_call_table = (unsigned long*)0xffffffffbb6001c0;

//puntero de la funcion del sys_openat
asmlinkage int (*real_unlinkat)(int DirFileDescriptor, char* path, int Flag);

//Reemplazando la llamada original con la llamada modificada
asmlinkage int custom_unlinkat(int DirFileDescriptor, char* path, int Flag)
{
	printk("interceptor: unlinkat(%d, \"%s\", %d)\n", DirFileDescriptor,path,Flag);
	return 0;
}

/*
Modificando la pagina de la memoria para escritura
Esto es un poco riesgoso ya que se modifica el bit de proteccion a nivel de arquitectura
*/
int make_rw(unsigned long address)
{
	unsigned int level;
	pte_t *pte = lookup_address(address, &level);
	if(pte->pte &~ _PAGE_RW){
		pte->pte |= _PAGE_RW;
		printk("RW seteado\n");
	}
	return 0;
}

/* Protegiendo la pagina contra escritura*/
int make_ro(unsigned long address)
{
	unsigned int level;
	pte_t *pte = lookup_address(address, &level);
	pte->pte = pte->pte &~ _PAGE_RW;
	printk("RO seteado\n");
	return 0;
}


static int __init init_my_module(void)
{
	//para este ejemplo se utiliza la llamada al sistema openat para abir archivos
	printk(KERN_INFO "Inside kernel space\n");
	//cambiando permisos de la pagina
	make_rw((unsigned long)sys_call_table);
	//guardando el valor de memoria de la llamada original
	real_unlinkat = (void *)sys_call_table[__NR_unlinkat];
	//insertando nuestra funcion a la direccion de memoria de openat
	*(sys_call_table + __NR_unlinkat) = (unsigned long)custom_unlinkat;
	printk("hizo el cambio de pagina");
	return 0;
}

static void __exit cleanup_my_module(void)
{
	
	//cambiando la direccion de memoria a modo de escritura
	make_rw((unsigned long)sys_call_table);
	//regresando la funcion original a la direccion de la llamada
	*(sys_call_table + __NR_unlinkat) = (unsigned long)real_unlinkat;
	//cambiando la direccion de memoria a modo de lectura. 
	make_ro((unsigned long)sys_call_table);
	printk(KERN_INFO "Exiting kernel space\n");
	return;
}

module_init(init_my_module);
module_exit(cleanup_my_module);