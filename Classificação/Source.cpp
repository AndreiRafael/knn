#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <vector>
#include <iostream>
#include <list>
#include <algorithm>
#include <time.h>
#include <unordered_map>
#include <thread>

#define DEBUG_MSG

enum MyClasses {
	KIDNEY_INTERIOR = 0,
	KIDNEY_BORDER,
	TRANSPARENTE,
	NO_CLASS
};

enum MyAttributes {
	ATTRIB_CLASS = 0,//a classe 
	ATTRIB_COLOR_R,
	ATTRIB_COLOR_G,
	ATTRIB_COLOR_B,
	ATTRIB_MAX_VALUE
};

enum TestClasses {
	TC_REDTOMATO = 0,
	TC_NOTREDTOMATO
};

Uint32 getpixel(SDL_Surface* surface, int x, int y)
{
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8* p = (Uint8*)surface->pixels + (y * surface->pitch + x * bpp);

	switch (bpp) {
	case 1:
		return *p;
		break;

	case 2:
		return *(Uint16*)p;
		break;

	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			return p[0] << 16 | p[1] << 8 | p[2];
		else
			return p[0] | p[1] << 8 | p[2] << 16;
		break;

	case 4:
		return *(Uint32*)p;
		break;

	default:
		return 0;       /* shouldn't happen, but avoids warnings */
	}
}

void putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to set */
	Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;

	switch (bpp) {
	case 1:
		*p = pixel;
		break;

	case 2:
		*(Uint16*)p = pixel;
		break;

	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p[0] = (pixel >> 16) & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = pixel & 0xff;
		}
		else {
			p[0] = pixel & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = (pixel >> 16) & 0xff;
		}
		break;

	case 4:
		*(Uint32*)p = pixel;
		break;
	}
}

///<summary>
///Classe que contém os atributos normalizados e a classe a qual essa Instância pertence
///</summary>
class NormalizedSample {
	std::vector<float> normalizedAtributes;//os atributos desse cara

public:
	NormalizedSample(size_t attributeCount) {
		normalizedAtributes.resize(attributeCount, 0.0f);
	}

	float getAttribute(size_t attributeIndex) const {
		if (attributeIndex >= normalizedAtributes.size()) {
			throw std::exception("Fora do tamanho do vetor!");
			return 0.0f;
		}

		return normalizedAtributes.at(attributeIndex);
	}

	void setAttribute(size_t attributeIndex, float newValue) {
		if (attributeIndex >= normalizedAtributes.size()) {
			throw std::exception("Fora do tamanho do vetor!");
			return;
		}

		if (newValue < 0.0f || newValue > 1.0f) {
			std::cout << "ATENCAO: O valor escolhido nao esta entre 0.0 e 1.0. Verifique se a normalizacao foi aplicada.\n";
		}

		normalizedAtributes.at(attributeIndex) = newValue;
	}

	float distanceTo(const NormalizedSample& other, size_t ignoredAttributeIndex) {
		float sum = 0;
		for (size_t i = 0; i < normalizedAtributes.size(); i++) {
			if (i == ignoredAttributeIndex) {
				continue;
			}

			sum += std::pow(this->getAttribute(i) - other.getAttribute(i), 2);
		}

		return std::sqrt(sum);
	}

	///<summary>Lista todas as classes presentes no array de amostras.</summary>
	static std::list<float> getAllAttributes(const std::vector<NormalizedSample>& samples, const size_t attributeIndex, const float epsilon = 0.01f) {
		std::list<float> classList = std::list<float>();

		for (auto sample : samples) {
			float attribValue = sample.getAttribute(attributeIndex);
			auto closeEnough = [epsilon, attribValue](float f) -> bool {
				return abs(f - attribValue) < epsilon;
			};

			if (std::find_if(classList.begin(), classList.end(), closeEnough) == classList.end()) {//Não está na lista, portanto adiciona.
				classList.push_back(sample.getAttribute(attributeIndex));
			}
		}

		return classList;
	}

	///<summary>Conta a occorencia de um mesmo valor em um atributo, em um vetor de amostras.</summary>
	static uint32_t getAttributeOccurenceCount(const std::vector<NormalizedSample> & samples, const float attributeValue, const size_t attributeIndex, const float epsilon = 0.01f) {
		uint32_t count = 0u;

		auto closeEnough = [epsilon](float a, float b) {
			return abs(a - b) < epsilon;
		};

		for (auto sample : samples) {
			if (closeEnough(sample.getAttribute(attributeIndex), attributeValue)) {
				count++;
			}
		}
		return count;
	}

	///<summary>Simplesmente retorna verdadeiro se as duas instâncias tem a mesma quantidade de atributos.</summary>
	static bool compareSamples(const NormalizedSample & a, const NormalizedSample & b) {
		return a.normalizedAtributes.size() == b.normalizedAtributes.size();
	}

	///<summary>Equaliza as amostras para que todas tenham a mesma quantia de um atributo que a menor.</summary>
	///<param name="samples">O vetor de amostras.</param>
	///<param name="attributeIndex">O índice do atributo usado na equalização.</param>
	///<returns>O novo vetor com amostras equalizadas.</returns>
	static std::vector<NormalizedSample> equalizeSamples(const std::vector<NormalizedSample> & samples, const size_t attributeIndex, const float epsilon = 0.1f) {
		auto attribList = getAllAttributes(samples, attributeIndex);
		std::vector<uint32_t> attribCounts;
		attribCounts.reserve(attribList.size());

		uint32_t min = std::numeric_limits<uint32_t>::max();
		for (auto attrib : attribList) {
			auto newCount = getAttributeOccurenceCount(samples, attrib, attributeIndex, epsilon);
			attribCounts.push_back(newCount);
			if (newCount < min) {
				min = newCount;
			}
		}

#ifdef DEBUG_MSG
		std::cout << "Minimum value determined, creating new Vector\n";
#endif

		auto returnVector = std::vector<NormalizedSample>();
		returnVector.reserve(min * attribList.size());

		auto countIterator = attribCounts.begin();
		for (auto sample : samples) {
			auto count = *(countIterator++);
			if (count < min) {
				returnVector.push_back(sample);
			}
		}

#ifdef DEBUG_MSG
		std::cout << "Samples equalized\n";
#endif

		return returnVector;
	}
};

///<summary>Avalia e atribui a classe desta instância por meio do KNN executado sobre o vetor de treinamento</summary>
void knnEvaluateAttribute(NormalizedSample * unevaluatedSample, std::vector<NormalizedSample> & samples, const size_t attributeIndex, const uint32_t kValue, const float epsilon = 0.01f) {
	std::list<NormalizedSample> samplesList = std::list<NormalizedSample>();

	for (auto sample : samples) {
		float dist = unevaluatedSample->distanceTo(sample, attributeIndex);

		auto iterator = samplesList.begin();
		while (iterator != samplesList.end() && iterator->distanceTo(*unevaluatedSample, attributeIndex) < dist) {
			iterator++;
		}
		samplesList.insert(iterator, sample);

		while (samplesList.size() > kValue) {
			samplesList.pop_back();
		}
	}

	std::unordered_map<float, uint32_t> votes;//votos (id, numero de votos)
	for (auto attributeValue : NormalizedSample::getAllAttributes(samples, attributeIndex, epsilon)) {
		votes.insert(std::pair<float, uint32_t>(attributeValue, 0u));
	}

	int kVisited = 0;
	for (auto sample : samplesList) {
		if (kVisited++ >= kValue) {
			break;
		}

		uint32_t attrib = (uint32_t)sample.getAttribute(attributeIndex);
		votes.insert_or_assign(attrib, votes.at(attrib) + 1u);
	}

	uint32_t maxVotes = 0;
	for (auto vote : votes) {//encontra quem teve maior votação
		if (vote.second >= maxVotes) {
			maxVotes = vote.second;
			unevaluatedSample->setAttribute(attributeIndex, vote.first);
		}
	}

#ifdef DEBUG_MSG
	std::cout << "Sample evaluated\n";
#endif
}

//lê uma imagem e define seus atributos de acordo com o gabarito
std::vector<NormalizedSample> lerBaseDeImagem(SDL_Surface* origem, SDL_Surface* gabarito) {
	std::vector<NormalizedSample> returnVector;
	returnVector.reserve(origem->w * origem->h);

	for (int x = 0; x < origem->w; x++) {
		for (int y = 0; y < origem->h; y++) {
			NormalizedSample newSample(ATTRIB_MAX_VALUE);

			uint8_t r = 0u, g = 0u, b = 0u;
			SDL_GetRGB(getpixel(origem, x, y), origem->format, &r, &g, &b);

			uint8_t classe = 0u;
			uint8_t placeholder;
			SDL_GetRGB(getpixel(gabarito, x, y), gabarito->format, &classe, &placeholder, &placeholder);

			newSample.setAttribute(ATTRIB_CLASS, (float)classe);
			newSample.setAttribute(ATTRIB_COLOR_R, (float)r / 255.0f);
			newSample.setAttribute(ATTRIB_COLOR_G, (float)g / 255.0f);
			newSample.setAttribute(ATTRIB_COLOR_B, (float)b / 255.0f);

			returnVector.push_back(newSample);

			std::cout << "( " << x << ", " << y << ")\n";
		}
	}

	return returnVector;
}

SDL_Surface* gerarGabarito(std::vector<NormalizedSample>& samples, SDL_Surface* origem, uint32_t kValue = 3u) {
	SDL_Surface* outSurf = SDL_CreateRGBSurfaceWithFormat(0, origem->w, origem->h, origem->format->BitsPerPixel, origem->format->format);

	for (int x = 0; x < origem->w; x++) {
		for (int y = 0; y < origem->h; y++) {
			NormalizedSample newSample(ATTRIB_MAX_VALUE);

			uint8_t r, g, b;
			SDL_GetRGB(getpixel(origem, x, y), origem->format, &r, &g, &b);

			newSample.setAttribute(ATTRIB_COLOR_R, (float)r / 255.0f);
			newSample.setAttribute(ATTRIB_COLOR_G, (float)g / 255.0f);
			newSample.setAttribute(ATTRIB_COLOR_B, (float)b / 255.0f);

			std::cout << "Gabaritando (" << x << ',' << y << ")\n";
			knnEvaluateAttribute(&newSample, samples, 0u, kValue);

			uint32_t pixel = SDL_MapRGB(outSurf->format, (uint32_t)newSample.getAttribute(0u), 0u, 0u);
			putpixel(outSurf, x, y, pixel);
		}
	}

	return outSurf;
}

int main(int argc, char* argv[]) {
	srand(time(NULL));
	SDL_Init(SDL_INIT_EVERYTHING);

	std::cout << "Gerando base...\n";
	auto samples = NormalizedSample::equalizeSamples(lerBaseDeImagem(IMG_Load("C:\\Users\\andre\\OneDrive\\Imagens\\MINER\\tomate_original_low.png"), IMG_Load("C:\\Users\\andre\\OneDrive\\Imagens\\MINER\\tomate_gabarito_low.png")), 0u);

	std::cout << "Gerando gabarito...\n";
	IMG_SavePNG(gerarGabarito(samples, IMG_Load("C:\\Users\\andre\\OneDrive\\Imagens\\MINER\\tomate_teste_low.png"), 7u), "C:\\Users\\andre\\OneDrive\\Imagens\\MINER\\high.png");

	getchar();
	return 0;
}