/*
 * For internal use only.
 *
 * Represents the security queries for which we currently have ML-powered versions.
 */

newtype TQuery =
  TNosqlInjectionQuery() or
  TSqlInjectionQuery() or
  TTaintedPathQuery() or
  TXssQuery() or
  TStoredXssQuery() or
  TXssThroughDomQuery() or
  TCodeInjectionQuery()

abstract class Query extends TQuery {
  abstract string getName();

  string toString() { result = getName() }
}

class NosqlInjectionQuery extends Query, TNosqlInjectionQuery {
  override string getName() { result = "NosqlInjection" }
}

class SqlInjectionQuery extends Query, TSqlInjectionQuery {
  override string getName() { result = "SqlInjection" }
}

class TaintedPathQuery extends Query, TTaintedPathQuery {
  override string getName() { result = "TaintedPath" }
}

class XssQuery extends Query, TXssQuery {
  override string getName() { result = "Xss" }
}

class StoredXssQuery extends Query, TStoredXssQuery {
  override string getName() { result = "StoredXss" }
}

class XssThroughDomQuery extends Query, TXssThroughDomQuery {
  override string getName() { result = "XssThroughDom" }
}

class CodeInjectionQuery extends Query, TCodeInjectionQuery {
  override string getName() { result = "CodeInjection" }
}