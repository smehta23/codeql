// generated by codegen/codegen.py
import codeql.swift.elements
import TestUtils

from
  NestedArchetypeType x, string getDiagnosticsName, Type getCanonicalType, string getName,
  Type getInterfaceType, ArchetypeType getParent, AssociatedTypeDecl getAssociatedTypeDeclaration
where
  toBeTested(x) and
  not x.isUnknown() and
  getDiagnosticsName = x.getDiagnosticsName() and
  getCanonicalType = x.getCanonicalType() and
  getName = x.getName() and
  getInterfaceType = x.getInterfaceType() and
  getParent = x.getParent() and
  getAssociatedTypeDeclaration = x.getAssociatedTypeDeclaration()
select x, "getDiagnosticsName:", getDiagnosticsName, "getCanonicalType:", getCanonicalType,
  "getName:", getName, "getInterfaceType:", getInterfaceType, "getParent:", getParent,
  "getAssociatedTypeDeclaration:", getAssociatedTypeDeclaration
