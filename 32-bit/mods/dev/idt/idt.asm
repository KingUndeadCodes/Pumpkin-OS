global IDTLoad

IDTLoad:
	[extern _IDTPointer]
	lidt [_IDTPointer]
	ret
