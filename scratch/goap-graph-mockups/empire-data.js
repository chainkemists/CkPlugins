// Empire gym GOAP data — mirrors CkGoapEmpire_Actions.as (45 actions, 4 goals).
// Preconditions are all implicit `== true`; effects are all implicit `→ true`.
// Category is purely for display grouping (gather/build/research/train/age/trade).

const EMPIRE_ACTIONS = [
  // Gather — raw + efficient variants
  { name: "GatherWood",             cat: "gather",  cost: 3, pre: [],                                                             eff: ["HasEnoughWood"] },
  { name: "GatherWoodFromCamp",     cat: "gather",  cost: 1, pre: ["HasLumberCamp"],                                              eff: ["HasEnoughWood"] },
  { name: "GatherFood",             cat: "gather",  cost: 3, pre: [],                                                             eff: ["HasEnoughFood"] },
  { name: "GatherFoodFromFarm",     cat: "gather",  cost: 1, pre: ["HasFarm"],                                                    eff: ["HasEnoughFood"] },
  { name: "GatherFoodFromMill",     cat: "gather",  cost: 2, pre: ["HasMill"],                                                    eff: ["HasEnoughFood"] },
  { name: "GatherGold",             cat: "gather",  cost: 3, pre: [],                                                             eff: ["HasEnoughGold"] },
  { name: "GatherGoldFromCamp",     cat: "gather",  cost: 1, pre: ["HasMiningCamp"],                                              eff: ["HasEnoughGold"] },
  { name: "GatherStone",            cat: "gather",  cost: 3, pre: [],                                                             eff: ["HasEnoughStone"] },
  { name: "GatherStoneFromCamp",    cat: "gather",  cost: 1, pre: ["HasMiningCamp"],                                              eff: ["HasEnoughStone"] },

  // Population
  { name: "BuildHouse",             cat: "build",   cost: 2, pre: ["HasEnoughWood"],                                              eff: ["HasHouse", "HasFreePopulation"] },
  { name: "TrainVillager",          cat: "train",   cost: 2, pre: ["HasEnoughFood", "HasFreePopulation"],                         eff: ["HasEnoughVillagers"] },

  // Economy buildings
  { name: "BuildLumberCamp",        cat: "build",   cost: 3, pre: ["HasEnoughWood"],                                              eff: ["HasLumberCamp"] },
  { name: "BuildMill",              cat: "build",   cost: 3, pre: ["HasEnoughWood"],                                              eff: ["HasMill"] },
  { name: "BuildMiningCamp",        cat: "build",   cost: 3, pre: ["HasEnoughWood"],                                              eff: ["HasMiningCamp"] },
  { name: "BuildFarm",              cat: "build",   cost: 2, pre: ["HasEnoughWood", "HasMill"],                                   eff: ["HasFarm"] },
  { name: "BuildDock",              cat: "build",   cost: 4, pre: ["HasEnoughWood"],                                              eff: ["HasDock"] },
  { name: "BuildMarket",            cat: "build",   cost: 4, pre: ["HasEnoughWood", "IsInFeudalAge"],                             eff: ["HasMarket"] },

  // Military buildings
  { name: "BuildBarracks",          cat: "build",   cost: 4, pre: ["HasEnoughWood"],                                              eff: ["HasBarracks"] },
  { name: "BuildArchery",           cat: "build",   cost: 4, pre: ["HasEnoughWood", "HasBarracks", "IsInFeudalAge"],              eff: ["HasArchery"] },
  { name: "BuildStable",            cat: "build",   cost: 4, pre: ["HasEnoughWood", "HasBarracks", "IsInFeudalAge"],              eff: ["HasStable"] },
  { name: "BuildBlacksmith",        cat: "build",   cost: 3, pre: ["HasEnoughWood", "IsInFeudalAge"],                             eff: ["HasBlacksmith"] },
  { name: "BuildMonastery",         cat: "build",   cost: 4, pre: ["HasEnoughWood", "IsInCastleAge"],                             eff: ["HasMonastery"] },
  { name: "BuildUniversity",        cat: "build",   cost: 5, pre: ["HasEnoughWood", "HasEnoughStone", "IsInCastleAge"],           eff: ["HasUniversity"] },
  { name: "BuildSiegeWorkshop",     cat: "build",   cost: 5, pre: ["HasEnoughWood", "IsInCastleAge"],                             eff: ["HasSiegeWorkshop"] },
  { name: "BuildCastle",            cat: "build",   cost: 6, pre: ["HasEnoughStone", "IsInCastleAge"],                            eff: ["HasCastle"] },
  { name: "BuildWonder",            cat: "build",   cost: 10, pre: ["HasEnoughWood", "HasEnoughStone", "HasEnoughGold", "IsInImperialAge"], eff: ["HasWonder"] },

  // Age advances
  { name: "AdvanceToFeudalAge",     cat: "age",     cost: 6,  pre: ["IsInDarkAge", "HasBarracks", "HasEnoughFood"],                                          eff: ["IsInFeudalAge"] },
  { name: "AdvanceToCastleAge",     cat: "age",     cost: 8,  pre: ["IsInFeudalAge", "HasBlacksmith", "HasEnoughFood", "HasEnoughGold"],                     eff: ["IsInCastleAge"] },
  { name: "AdvanceToImperialAge",   cat: "age",     cost: 10, pre: ["IsInCastleAge", "HasUniversity", "HasEnoughFood", "HasEnoughGold"],                     eff: ["IsInImperialAge"] },

  // Research
  { name: "ResearchLoom",           cat: "research", cost: 3, pre: ["HasEnoughFood"],                                                                        eff: ["HasResearchedLoom"] },
  { name: "ResearchWheelbarrow",    cat: "research", cost: 3, pre: ["HasMill", "IsInFeudalAge", "HasEnoughFood", "HasEnoughGold"],                           eff: ["HasResearchedWheelbarrow"] },
  { name: "ResearchHandCart",       cat: "research", cost: 4, pre: ["HasResearchedWheelbarrow", "IsInCastleAge", "HasEnoughFood", "HasEnoughGold"],          eff: ["HasResearchedHandCart"] },
  { name: "ResearchGoldMining",     cat: "research", cost: 3, pre: ["HasMiningCamp", "IsInFeudalAge", "HasEnoughFood"],                                      eff: ["HasResearchedGoldMining"] },
  { name: "ResearchStoneMining",    cat: "research", cost: 3, pre: ["HasMiningCamp", "IsInFeudalAge", "HasEnoughFood"],                                      eff: ["HasResearchedStoneMining"] },
  { name: "ResearchCrossbow",       cat: "research", cost: 4, pre: ["HasArchery", "IsInFeudalAge", "HasEnoughFood", "HasEnoughGold"],                        eff: ["HasResearchedCrossbow"] },
  { name: "ResearchFletching",      cat: "research", cost: 3, pre: ["HasBlacksmith", "IsInFeudalAge", "HasEnoughFood", "HasEnoughGold"],                     eff: ["HasResearchedFletching"] },
  { name: "ResearchHusbandry",      cat: "research", cost: 3, pre: ["HasStable", "IsInFeudalAge", "HasEnoughFood"],                                          eff: ["HasResearchedHusbandry"] },
  { name: "ResearchBloodlines",     cat: "research", cost: 4, pre: ["HasStable", "IsInFeudalAge", "HasEnoughFood", "HasEnoughGold"],                         eff: ["HasResearchedBloodlines"] },
  { name: "ResearchEliteSkirmisher",cat: "research", cost: 4, pre: ["HasResearchedCrossbow", "IsInCastleAge", "HasEnoughFood", "HasEnoughGold"],             eff: ["HasResearchedEliteSkirmisher"] },
  { name: "ResearchChemistry",      cat: "research", cost: 5, pre: ["HasUniversity", "IsInImperialAge", "HasEnoughFood", "HasEnoughGold"],                   eff: ["HasResearchedChemistry"] },
  { name: "ResearchBallistics",     cat: "research", cost: 4, pre: ["HasUniversity", "IsInCastleAge", "HasEnoughFood", "HasEnoughGold"],                     eff: ["HasResearchedBallistics"] },

  // Trade
  { name: "TradeForFood",           cat: "trade",    cost: 2, pre: ["HasMarket", "HasEnoughGold"],                                                           eff: ["HasEnoughFood"] },

  // Unit training
  { name: "TrainMilitia",           cat: "train",    cost: 2, pre: ["HasBarracks", "HasEnoughFood", "HasFreePopulation"],                                                            eff: ["HasArmyLight"] },
  { name: "TrainSpearman",          cat: "train",    cost: 2, pre: ["HasBarracks", "IsInFeudalAge", "HasEnoughFood", "HasFreePopulation"],                                            eff: ["HasArmyLight"] },
  { name: "TrainArcher",            cat: "train",    cost: 3, pre: ["HasArchery", "IsInFeudalAge", "HasEnoughFood", "HasEnoughGold", "HasFreePopulation"],                            eff: ["HasArmyRanged"] },
  { name: "TrainKnight",            cat: "train",    cost: 4, pre: ["HasStable", "HasResearchedBloodlines", "IsInCastleAge", "HasEnoughFood", "HasEnoughGold", "HasFreePopulation"],  eff: ["HasArmyCavalry", "HasArmyHeavy"] },
  { name: "TrainMonk",              cat: "train",    cost: 3, pre: ["HasMonastery", "IsInCastleAge", "HasEnoughGold", "HasFreePopulation"],                                           eff: ["HasArmyHeavy"] },
  { name: "TrainSiegeRam",          cat: "train",    cost: 5, pre: ["HasSiegeWorkshop", "IsInCastleAge", "HasEnoughFood", "HasEnoughGold", "HasFreePopulation"],                      eff: ["HasArmySiege"] },
  { name: "TrainTrebuchet",         cat: "train",    cost: 6, pre: ["HasCastle", "HasResearchedBallistics", "IsInImperialAge", "HasEnoughFood", "HasEnoughGold", "HasFreePopulation"], eff: ["HasArmySiege"] },
];

const EMPIRE_GOALS = [
  { name: "ReachFeudalAge",   priority: 10, cond: ["IsInFeudalAge"] },
  { name: "ReachCastleAge",   priority: 8,  cond: ["IsInCastleAge"] },
  { name: "ReachImperialAge", priority: 6,  cond: ["IsInImperialAge"] },
  { name: "BuildWonder",      priority: 4,  cond: ["HasWonder"] },
];

const CATEGORY_COLORS = {
  gather:   "#4ea84e",
  build:    "#c28a2a",
  research: "#5fb3d4",
  train:    "#c74c4c",
  age:      "#b46fd0",
  trade:    "#d4b15f",
  goal:     "#f5c842",
};

const AGE_ORDER = ["Dark", "Feudal", "Castle", "Imperial"];

function getAgeGate(action) {
  for (const p of action.pre) {
    if (p === "IsInImperialAge") return 3;
    if (p === "IsInCastleAge")   return 2;
    if (p === "IsInFeudalAge")   return 1;
    if (p === "IsInDarkAge")     return 0;
  }
  return 0;
}

// Goal-distance: shortest number of action edges from this action's effects to
// any goal condition. Used for layered DAG depth.
function computeGoalDistances(actions, goals) {
  const goalConds = new Set(goals.flatMap(g => g.cond));
  // effect → set of actions that produce it
  const producers = new Map();
  for (const a of actions) {
    for (const e of a.eff) {
      if (!producers.has(e)) producers.set(e, []);
      producers.get(e).push(a.name);
    }
  }
  const dist = new Map();
  const frontier = [];
  for (const a of actions) {
    if (a.eff.some(e => goalConds.has(e))) {
      dist.set(a.name, 0);
      frontier.push(a.name);
    }
  }
  let head = 0;
  while (head < frontier.length) {
    const name = frontier[head++];
    const a = actions.find(x => x.name === name);
    const d = dist.get(name);
    for (const p of a.pre) {
      const producerList = producers.get(p) || [];
      for (const prodName of producerList) {
        if (!dist.has(prodName)) {
          dist.set(prodName, d + 1);
          frontier.push(prodName);
        }
      }
    }
  }
  for (const a of actions) {
    if (!dist.has(a.name)) dist.set(a.name, -1); // unreachable from goals
  }
  return dist;
}

if (typeof module !== "undefined") {
  module.exports = { EMPIRE_ACTIONS, EMPIRE_GOALS, CATEGORY_COLORS, AGE_ORDER, getAgeGate, computeGoalDistances };
}
