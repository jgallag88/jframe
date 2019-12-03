Given a program counter and a java core, output the function name corresponding to the program counter

Example:
```
~/jframe$ cat ../umausers.out
0xfffffd7fecb65fcb
0xfffffd7fecb1b434
0xfffffd7fed154b34
0xfffffd7fed15ff04
0xfffffd7fed761d8c
0xfffffd7fecd0e5e0
0xfffffd7fec8004e7
~/jframe$ cat ../umausers.out | ./jframe ../core.1189
*java/lang/ClassLoader.defineClass1(Ljava/lang/String;[BIILjava/security/ProtectionDomain;Ljava/lang/String;)Ljava/lang/Class; [compiled]
*java/util/Collections$SynchronizedMap.<init>(Ljava/util/Map;)V [compiled]
*org/springframework/beans/factory/support/AbstractAutowireCapableBeanFactory.initializeBean(Ljava/lang/String;Ljava/lang/Object;Lorg/springframework/beans/factory/support/RootBeanDefinition;)Ljava/lang/Object; [compiled]
*com/sun/org/apache/xerces/internal/impl/xs/XMLSchemaValidator.processAttributes(Lcom/sun/org/apache/xerces/internal/xni/QName;Lcom/sun/org/apache/xerces/internal/xni/XMLAttributes;Lcom/sun/org/apache/xerces/internal/impl/xs/XSAttributeGroupDecl;)V [compil
*com/fasterxml/jackson/core/sym/CharsToNameCanonicalizer.findSymbol([CIII)Ljava/lang/String; [compiled]
*org/apache/catalina/loader/WebappClassLoaderBase.loadClass(Ljava/lang/String;)Ljava/lang/Class; [compiled]
StubRoutines (1)
```
