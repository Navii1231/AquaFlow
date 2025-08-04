#include "Core/Aqpch.h"
#include "MaterialSystem/MaterialPostprocessor.h"

std::expected<std::string, AQUA_NAMESPACE::MAT_NAMESPACE::MaterialGraphPostprocessError>
	AQUA_NAMESPACE::MaterialPostprocessor::ResolveCustomParameters(MAT_NAMESPACE::ShaderParameterSet& parameters) const
{
	if (mShaderTextView.empty())
		return std::unexpected(ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eEmptyString, {}, "Empty string"));

	Lexer lexer;
	lexer.SetString(mShaderTextView);
	lexer.SetWhiteSpacesAndDelimiters(" \t\r\n", "@.(){}[]=+-*/%|^&!~?:;,");

	Token token;

	size_t prevIdx = lexer.GetPosition();

	std::string output;
	output.reserve(mShaderTextView.size());

	while(true)
	{
		prevIdx = lexer.GetPosition();
		token = lexer.Advance();

		if (token.Lexeme.empty())
		{
			output += mShaderTextView.substr(prevIdx, lexer.GetPosition() - prevIdx);
			break;
		}

		// Look for '@type', '.', '_', 'index' pattern
		if (token.Lexeme.front() == mParIdentifier)
		{
			Token typeName = lexer.Advance(); // skip '@'
			Token dot = lexer.Advance();
			Token varName = lexer.Advance();

			if (!ValidVariableName(varName.Lexeme))
				return std::unexpected(ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidCustomParameter,
					varName, "Invalid custom parameter name: " + std::string(varName.Lexeme)));

			if (IsBasicShaderType(typeName.Lexeme) && dot.Lexeme == ".")
			{
				lexer.Advance(); // consume index

				auto& par = parameters[std::string(varName.Lexeme)];
				par.Type = std::string(typeName.Lexeme);
				par.Name = mCustomPrefix + std::string(varName.Lexeme);
				par.TypeSize = 0; // will be set by the material builder
				par.Offset = 0; // will be configured by the material builder

				output += par.Name;
				continue;
			}

			return std::unexpected(ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidCustomParameter,
					token, "Invalid Custom parameter: Unexpected token"));
		}

		// Otherwise just emit the token as is
		output += mShaderTextView.substr(prevIdx, lexer.GetPosition() - prevIdx);
	}

	return output;
}

std::expected<std::string, AQUA_NAMESPACE::MAT_NAMESPACE::MaterialGraphPostprocessError>
	AQUA_NAMESPACE::MaterialPostprocessor::ResolveImportDirectives() const
{
	// An import system to load pre-existing shaders...
	// Using a lexer for that as we will use custom key word for inclusion
	// The custom keyword and grammar for inclusion is:
	// import [ShaderName]

	std::string output;
	output.reserve(mShaderTextView.size());

	MAT_NAMESPACE::MaterialGraphPostprocessError error;
	error.State = MAT_NAMESPACE::MaterialPostprocessState::eSuccess;

	Lexer lexer;
	lexer.SetString(mShaderTextView);
	lexer.SetWhiteSpacesAndDelimiters(" \t\r", "\n");

	size_t prevIdx = 0;

	Token PrevToken{};
	Token CurrToken{};

	while (true)
	{
		prevIdx = lexer.GetPosition();

		PrevToken = CurrToken;
		CurrToken = lexer.Advance();

		if (CurrToken.Lexeme.empty())
		{
			output += mShaderTextView.substr(prevIdx, lexer.GetPosition() - prevIdx);
			break;
		}

		if (CurrToken.Lexeme == "import")
		{
			Token ShaderNameToken = lexer.Advance();
			Token EndLineToken = lexer.Advance();

			if (!ImportSanityCheck(error, PrevToken, CurrToken, ShaderNameToken, EndLineToken))
				return std::unexpected(error);

			std::string_view shaderName = ShaderNameToken.Lexeme;

			size_t position = CurrToken.PosInStr;
			size_t Size = CurrToken.Lexeme.size() + ShaderNameToken.Lexeme.size() + EndLineToken.Lexeme.size();

			std::string lexemeString = std::string(shaderName);

			auto iter = mImportsModules.find(lexemeString);

			if (iter == mImportsModules.end())
			{
				ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eShaderNotFound, CurrToken,
					"Could not import the shader: " + lexemeString);

				return std::unexpected(error);
			}

			// adding the module text here
			output += iter->second;
			output += "\n";

			PrevToken = CurrToken;
			CurrToken = lexer.Advance();

			continue;
		}

		// Otherwise just emit the token as is
		output += mShaderTextView.substr(prevIdx, lexer.GetPosition() - prevIdx);
	}

	return output;
}

AQUA_NAMESPACE::MAT_NAMESPACE::MaterialGraphPostprocessError AQUA_NAMESPACE::MaterialPostprocessor::ConstructError(
	MAT_NAMESPACE::MaterialPostprocessState state, const Token& token, const std::string& errorString) const
{
	MAT_NAMESPACE::MaterialGraphPostprocessError error;
	error.State = state;
	error.Info = "line (" + std::to_string(token.LineNo) + ", " +
		std::to_string(token.CharOff) + ") -- " + errorString;

	return error;
}

bool AQUA_NAMESPACE::MaterialPostprocessor::ImportSanityCheck(MAT_NAMESPACE::MaterialGraphPostprocessError& error,
	const Token& prevToken, const Token& importKeyword, const Token& shaderImport, const Token& endLine) const
{
	if (shaderImport.Lexeme.empty())
	{
		error = ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidImportDirective, importKeyword,
			"No shader has been provided");

		return false;
	}

	if (prevToken.Lexeme != "\n" && prevToken.Lexeme != importKeyword.Lexeme && !prevToken.Lexeme.empty())
	{
		error = ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidImportDirective, prevToken,
			"The import statement must begin with a new line");

		return false;
	}

	if (endLine.Lexeme != "\n" && endLine.Lexeme[0] != '\0')
	{
		error = ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidImportDirective, endLine,
			"Unexpected token \'" + std::string(endLine.Lexeme) + "\'");

		return false;
	}

	return true;
}

bool AQUA_NAMESPACE::MaterialPostprocessor::IsBasicShaderType(std::string_view type) const
{
	auto found = std::find(mBasicShaderTypes.begin(), mBasicShaderTypes.end(), std::string(type));

	if (found == mBasicShaderTypes.end())
		return false;

	return true;
}

bool AQUA_NAMESPACE::MaterialPostprocessor::ValidVariableName(std::string_view varName) const
{
	if (varName.empty())
		return false;

	if (!std::isalpha(varName.front()) && varName.front() != '_')
		return false;

	for (auto c : varName)
	{
		if (!std::isalpha(c) && !std::isdigit(c) && c != '_')
			return false;
	}

	return true;
}
