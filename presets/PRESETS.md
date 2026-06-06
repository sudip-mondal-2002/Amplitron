## 05. 4-Cable Method (4CM)

The 4-Cable Method is an industry-standard routing technique that takes advantage of our DAG architecture. It separates your signal chain into two distinct phases: **Pre-Amp** (Front of House) and **Post-Amp** (FX Loop). 

By placing dynamics and drive pedals (Compressor, Overdrive, Wah) *before* the Amp Simulator, you drive the input stage naturally. By placing time-based effects (Delay, Reverb) *after* the Amp Simulator, you ensure your echoes and reflections remain clean and un-muddied by the amp's distortion.

### Signal Flow

```mermaid
graph LR
%% Styling
classDef io fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff;
classDef preamp fill:#9b2c2c,stroke:#f56565,stroke-width:2px,color:#fff;
classDef amp fill:#744210,stroke:#ecc94b,stroke-width:2px,color:#fff;
classDef fxloop fill:#2b6cb0,stroke:#63b3ed,stroke-width:2px,color:#fff;

%% Nodes
In([🎸 Input]):::io
Out([🔊 Output]):::io

subgraph Pre-Amp Effects [Pre-Amp / Front of House]
    Comp(Compressor):::preamp
    OD(Overdrive):::preamp
    Wah(Wah-Wah):::preamp
end

Amp[Amp Simulator]:::amp

subgraph FX Loop [Post-Amp / FX Loop]
    Delay(Delay):::fxloop
    Reverb(Reverb):::fxloop
end

%% Connections
In --> Comp
Comp --> OD
OD --> Wah
Wah --> Amp
Amp --> Delay
Delay --> Reverb
Reverb --> Out
