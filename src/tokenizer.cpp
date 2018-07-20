#include "tokenizer.h"

#include <algorithm>

TokenizerRule::TokenizerRule(std::string type)
{
	this->type = type;
}

Token::Token(std::string str, std::string type)
{
	this->str = str;
	this->type = type;
}

Tokenizer::Tokenizer(bool trim)
{
	this->trim = trim;
}

void Tokenizer::addRule(TokenizerRule* rule)
{
	rules.push_back(rule);
}

KeywordRule::KeywordRule(std::string regex, std::string type, bool strict)
	: TokenizerRule(type)
{
	this->regex = regex;
	this->strict = strict;
}

std::vector<int> KeywordRule::getIndices(std::string input, int fromIndex)
{
	int index = input.find(regex, fromIndex);
	while (index != -1)
	{
		const char* chars = input.c_str();
		// Check if the word is supposed to be isolated from the rest of the text
		bool isolatedLeft = index == 0 || chars[index - 1] == ' ' || chars[index - 1] == '\t' || chars[index - 1] == '\n';
		bool isolatedRight = index + regex.length() == input.length() || chars[index + regex.length()] == ' ' || chars[index + regex.length()] == '\t' || chars[index + regex.length()] == '\n';
		bool isolated = isolatedLeft && isolatedRight;
		if (!strict || isolated)
		{
			int length = regex.length();
			return { index, index + length };
		}
		else
		{
			index = input.find(regex, index + regex.length());
		}
	}

	return std::vector<int>();
}

std::string KeywordRule::getSequence(std::string token, std::vector<int> indices)
{
	return token.substr(indices[0], indices[1] - indices[0]);
}

BeginRule::BeginRule(std::string begin, std::string type, bool fearWhitespace)
	: TokenizerRule(type)
{
	this->begin = begin;
	this->fearWhitespace = fearWhitespace;
}

std::vector<int> BeginRule::getIndices(std::string input, int fromIndex)
{
	int beginIndex = input.find(begin, fromIndex);
	int whitespaceIndex = fearWhitespace ? input.find(' ', beginIndex + 1) : -1;
	if (beginIndex != -1)
	{
		return { beginIndex, whitespaceIndex == -1 ? (int)input.length() : whitespaceIndex + 1 };
	}
	return std::vector<int>();
}

std::string BeginRule::getSequence(std::string token, std::vector<int> indices)
{
	return token.substr(indices[0], indices[1] - indices[0]);
}

BeginEndRule::BeginEndRule(std::string begin, std::string end, std::string type)
	: TokenizerRule(type)
{
	this->begin = begin;
	this->end = end;
}

std::vector<int> BeginEndRule::getIndices(std::string input, int fromIndex)
{
	int beginIndex = input.find(begin, fromIndex);
	int endIndex = input.find(end, beginIndex + 1) + 1;
	if (beginIndex != -1 && endIndex > beginIndex)
	{
		return { beginIndex, endIndex };
	}
	return std::vector<int>();
}

std::string BeginEndRule::getSequence(std::string token, std::vector<int> indices)
{
	return token.substr(indices[0], indices[1] - indices[0]);
}

std::vector<Token> Tokenizer::tokenize(std::string source)
{
	std::vector<Token> tokens;
	Token startToken(source, "null");
	tokens.push_back(startToken);

	int ruleIndex = 0;
	std::vector<Token> processedTokens = processRule(startToken, ruleIndex);

	if (trim)
	{
		skipWhitespace(processedTokens);
	}

	return processedTokens;
}

std::vector<Token> Tokenizer::processRule(Token token, int ruleIndex)
{
	if (ruleIndex >= (int)rules.size())
	{
		return std::vector<Token> { token };
	}

	std::vector<Token> newTokens;
	TokenizerRule* rule = rules[ruleIndex];

	std::vector<int> lastIndices;
	std::vector<int> currentIndices;

	bool found = false;
	while (!(currentIndices = rule->getIndices(token.str, currentIndices.empty() ? 0 : currentIndices[1])).empty())
	{
		found = true;

		if (lastIndices.empty())
		{
			if (currentIndices[0] > 0)
			{
				// Add two tokens
				Token preRegexToken = Token(token.str.substr(0, currentIndices[0]), token.type);
				Token regexToken = Token(rule->getSequence(token.str, currentIndices), rule->type);

				std::vector<Token> processedTokens = processRule(preRegexToken, ruleIndex + 1);
				if (!processedTokens.empty())
				{
					newTokens.insert(newTokens.end(), processedTokens.begin(), processedTokens.end());
				}
				else
				{
					newTokens.push_back(preRegexToken);
				}

				newTokens.push_back(regexToken);
			}
			else
			{
				// Add this token
				Token regexToken = Token(rule->getSequence(token.str, currentIndices), rule->type);
				newTokens.push_back(regexToken);
			}
		}
		else
		{
			int lastTokenEnd = lastIndices[1];
			if (currentIndices[0] > lastTokenEnd)
			{
				// Add two tokens
				Token preRegexToken = Token(token.str.substr(lastTokenEnd, currentIndices[0] - lastTokenEnd), token.type);
				Token regexToken = Token(rule->getSequence(token.str, currentIndices), rule->type);

				std::vector<Token> processedTokens = processRule(preRegexToken, ruleIndex + 1);
				if (!processedTokens.empty())
				{
					newTokens.insert(newTokens.end(), processedTokens.begin(), processedTokens.end());
				}
				else
				{
					newTokens.push_back(preRegexToken);
				}

				newTokens.push_back(regexToken);
			}
			else
			{
				// Add this token
				Token regexToken = Token(rule->getSequence(token.str, currentIndices), rule->type);
				newTokens.push_back(regexToken);
			}
		}

		lastIndices = currentIndices;

		if (currentIndices[1] < (int)token.str.length() && rule->getIndices(token.str, currentIndices[1]).empty())
		{
			// This was the last found occurence of the regex
			Token lastToken = Token(token.str.substr(currentIndices[1]), token.type);

			std::vector<Token> processedTokens = processRule(lastToken, ruleIndex + 1);
			if (!processedTokens.empty())
			{
				newTokens.insert(newTokens.end(), processedTokens.begin(), processedTokens.end());
			}
			else
			{
				newTokens.push_back(lastToken);
			}

			break;
		}
	}

	if (!found)
	{
		std::vector<Token> processedTokens = processRule(token, ruleIndex + 1);
		if (!processedTokens.empty())
		{
			newTokens.insert(newTokens.end(), processedTokens.begin(), processedTokens.end());
		}
		else
		{
			newTokens.push_back(token);
		}
	}

	return newTokens;
}

void Tokenizer::skipWhitespace(std::vector<Token>& tokens)
{
	for (unsigned int i = 0; i < tokens.size(); i++)
	{
		std::string str = tokens[i].str;
		str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\t'), str.end());
		str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
		if (str.empty())
		{
			tokens.erase(tokens.begin() + i);
			i--;
		}
	}
}